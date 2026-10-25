#!/usr/bin/env python3
"""Addestra la CNN di postura e la esporta per il firmware.

Ingresso della rete: immagine 40x30 a 2 canali (grigio e maschera di primo
piano, entrambi ridotti da 80x60), uscita: 4 classi
    0 NONE      nessuna persona
    1 STANDING  in piedi
    2 BENT      seduto, chinato, in ginocchio
    3 LYING     a terra

Dati:
  * sintetici: scene generate con lo stesso approccio dei test (sfondo con
    texture e rumore, persona come insieme di forme)
  * reali (consigliati): file .bin salvati dal dispositivo (pagina web ->
    "Salva fotogramma" oppure GET /frame.bin) in data/real/<classe>/

Uso:
  pip install tensorflow-cpu numpy
  python tools/train_posture_cnn.py                  # solo sintetici
  python tools/train_posture_cnn.py --real data/real # sintetici + reali
  python tools/train_posture_cnn.py --epochs 30 --synthetic 40000

Produce:
  models/posture_cnn.tflite       modello quantizzato int8
  src/posture_model_data.cc       lo stesso modello come array C
"""
import argparse
import os
import pathlib
import random
import sys

import numpy as np

W, H = 80, 60          # risoluzione di analisi del firmware
IN_W, IN_H = 40, 30    # ingresso della rete
CLASSES = ["NONE", "STANDING", "BENT", "LYING"]
DIFF_THRESHOLD = 28    # come fall::Params::diffThreshold

ROOT = pathlib.Path(__file__).resolve().parent.parent


# --------------------------------------------------------------- dati sintetici

def background(rng):
    """Sfondo con gradiente, texture e rumore."""
    y, x = np.mgrid[0:H, 0:W]
    base = rng.uniform(40, 140)
    grad = (x * rng.uniform(-0.6, 0.6) + y * rng.uniform(-0.6, 0.6))
    tex = ((x * rng.integers(1, 9) + y * rng.integers(1, 7)) % rng.integers(10, 60)) * rng.uniform(0.2, 1.0)
    img = base + grad + tex
    # qualche "mobile": rettangoli di luminosità diversa
    for _ in range(rng.integers(0, 4)):
        x0, y0 = rng.integers(0, W - 10), rng.integers(0, H - 10)
        img[y0:y0 + rng.integers(5, 30), x0:x0 + rng.integers(5, 40)] += rng.uniform(-40, 40)
    return np.clip(img, 0, 255)


def draw_person(img, mask, rng, label):
    """Disegna una persona con la postura richiesta. Restituisce False se non c'è spazio."""
    scale = rng.uniform(0.35, 1.0)  # distanza dalla telecamera
    if label == "STANDING":
        h = rng.uniform(26, 50) * scale
        w = h / rng.uniform(2.2, 4.0)
    elif label == "BENT":
        h = rng.uniform(16, 34) * scale
        w = h / rng.uniform(0.75, 1.6)
    else:  # LYING
        w = rng.uniform(26, 56) * scale
        h = w / rng.uniform(2.0, 4.5)
        if rng.random() < 0.25:       # caduta in scorcio: sagoma quasi quadrata ma bassa
            h = w / rng.uniform(1.0, 1.6)
            w = w * 0.8
    h, w = max(h, 5), max(w, 4)
    bottom = rng.uniform(H * 0.55, H - 1)
    if label == "LYING":
        bottom = rng.uniform(H * 0.7, H - 1)
    cx = rng.uniform(w / 2, W - w / 2)
    y1, y0 = int(bottom), int(bottom - h)
    x0, x1 = int(cx - w / 2), int(cx + w / 2)
    y0c, x0c = max(y0, 0), max(x0, 0)
    if y1 - y0c < 4 or x1 - x0c < 3:
        return False

    shade = rng.uniform(-110, 110)
    if abs(shade) < 35:                # troppo simile allo sfondo: rendilo visibile
        shade = 35 * np.sign(shade or 1)
    yy, xx = np.mgrid[y0c:y1 + 1, x0c:x1 + 1]
    if label == "STANDING":
        # tronco + testa più stretta
        body = np.ones_like(yy, dtype=bool)
        head_h = int(0.18 * (y1 - y0))
        body[yy < y0 + head_h] = np.abs(xx[yy < y0 + head_h] - cx) < w * 0.28
    elif label == "BENT":
        # ellisse (seduto/chinato)
        body = (((xx - cx) / (w / 2)) ** 2 + ((yy - (y0 + y1) / 2) / (h / 2)) ** 2) <= 1.0
    else:
        body = (((xx - cx) / (w / 2)) ** 2 + ((yy - (y0 + y1) / 2) / (h / 2)) ** 2) <= 1.0
        body |= rng.random() < 0.5 and (np.abs(yy - (y0 + y1) / 2) < h * 0.3)
    region = img[y0c:y1 + 1, x0c:x1 + 1]
    region[body] = np.clip(region[body] + shade + rng.normal(0, 6, body.sum()), 0, 255)
    mask[y0c:y1 + 1, x0c:x1 + 1][body] = 1
    return True


def synthetic_sample(rng, label):
    bg = background(rng)
    img = bg.copy()
    mask = np.zeros((H, W), np.uint8)
    if label != "NONE":
        if not draw_person(img, mask, rng, label):
            return None
    # oggetti di disturbo di primo piano (ombre, animali, tende) in tutte le classi
    for _ in range(rng.integers(0, 2)):
        x0, y0 = rng.integers(0, W - 6), rng.integers(0, H - 6)
        hh, ww = rng.integers(3, 10), rng.integers(3, 12)
        img[y0:y0 + hh, x0:x0 + ww] += rng.uniform(-60, 60)
    img = np.clip(img + rng.normal(0, 4, img.shape), 0, 255)
    # maschera come la calcola il firmware: differenza con lo sfondo sopra soglia
    fg_mask = (np.abs(img - bg) > DIFF_THRESHOLD).astype(np.uint8)
    # il firmware non ha lo sfondo perfetto: unisci maschera ideale e calcolata
    fg_mask = np.maximum(fg_mask, mask) if rng.random() < 0.7 else fg_mask
    return img.astype(np.uint8), fg_mask


def make_synthetic(n, seed):
    rng = np.random.default_rng(seed)
    xs, ys = [], []
    weights = [0.22, 0.30, 0.24, 0.24]
    while len(xs) < n:
        label = rng.choice(len(CLASSES), p=weights)
        s = synthetic_sample(rng, CLASSES[label])
        if s is None:
            continue
        xs.append(to_input(*s))
        ys.append(label)
    return np.stack(xs), np.array(ys, np.int64)


# ------------------------------------------------------------------ dati reali

def load_real(folder):
    """Carica i .bin (80x60 grigio + 80x60 maschera) da sottocartelle con il nome della classe."""
    xs, ys = [], []
    for ci, name in enumerate(CLASSES):
        d = pathlib.Path(folder) / name
        if not d.is_dir():
            continue
        for f in sorted(d.glob("*.bin")):
            raw = np.fromfile(f, np.uint8)
            if raw.size != 2 * W * H:
                print(f"salto {f}: dimensione {raw.size} (attesa {2 * W * H})", file=sys.stderr)
                continue
            gray = raw[: W * H].reshape(H, W)
            mask = raw[W * H:].reshape(H, W)
            xs.append(to_input(gray, mask))
            ys.append(ci)
            # aumento: specchiatura orizzontale
            xs.append(to_input(gray[:, ::-1], mask[:, ::-1]))
            ys.append(ci)
    if not xs:
        return None, None
    return np.stack(xs), np.array(ys, np.int64)


# ---------------------------------------------------------------- pre-processing

def to_input(gray, mask):
    """80x60 -> 40x30 a 2 canali, valori 0..255 (la quantizzazione li mappa su int8)."""
    g = gray.reshape(IN_H, 2, IN_W, 2).mean(axis=(1, 3))
    m = mask.reshape(IN_H, 2, IN_W, 2).max(axis=(1, 3)) * 255.0
    return np.stack([g, m], axis=-1).astype(np.float32)


# ------------------------------------------------------------------- modello

def build_model():
    import tensorflow as tf
    L = tf.keras.layers
    inp = L.Input(shape=(IN_H, IN_W, 2))
    x = L.Rescaling(1.0 / 255.0)(inp)
    x = L.Conv2D(8, 3, padding="same", activation="relu")(x)
    x = L.MaxPooling2D()(x)                      # 15x20
    x = L.Conv2D(16, 3, padding="same", activation="relu")(x)
    x = L.MaxPooling2D()(x)                      # 7x10
    x = L.Conv2D(32, 3, padding="same", activation="relu")(x)
    x = L.MaxPooling2D()(x)                      # 3x5
    x = L.Conv2D(32, 3, padding="same", activation="relu")(x)
    x = L.GlobalAveragePooling2D()(x)
    x = L.Dropout(0.2)(x)
    out = L.Dense(len(CLASSES), activation="softmax")(x)
    return tf.keras.Model(inp, out)


def export(model, x_rep, out_tflite, out_cc):
    import tensorflow as tf
    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]

    def rep():
        for i in range(0, min(len(x_rep), 500)):
            yield [x_rep[i:i + 1]]

    conv.representative_dataset = rep
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    data = conv.convert()
    out_tflite.parent.mkdir(parents=True, exist_ok=True)
    out_tflite.write_bytes(data)

    lines = [
        "// Generato da tools/train_posture_cnn.py - non modificare a mano.",
        "#include \"posture_model_data.h\"",
        "",
        "alignas(16) const unsigned char g_posture_model_data[] = {",
    ]
    for i in range(0, len(data), 16):
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
    lines += ["};", f"const unsigned int g_posture_model_data_len = {len(data)};", ""]
    out_cc.write_text("\n".join(lines))
    return len(data)


def evaluate_tflite(path, x, y):
    import tensorflow as tf
    interp = tf.lite.Interpreter(model_path=str(path))
    interp.allocate_tensors()
    inp = interp.get_input_details()[0]
    out = interp.get_output_details()[0]
    scale, zero = inp["quantization"]
    correct = 0
    conf = np.zeros((len(CLASSES), len(CLASSES)), int)
    for xi, yi in zip(x, y):
        q = np.clip(np.round(xi / scale + zero), -128, 127).astype(np.int8)
        interp.set_tensor(inp["index"], q[None])
        interp.invoke()
        pred = int(np.argmax(interp.get_tensor(out["index"])[0]))
        conf[yi, pred] += 1
        correct += pred == yi
    return correct / len(y), conf


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--real", help="cartella con sottocartelle NONE/STANDING/BENT/LYING di file .bin")
    ap.add_argument("--synthetic", type=int, default=24000, help="numero di esempi sintetici")
    ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--out-dir", default=str(ROOT / "models"))
    args = ap.parse_args()

    import tensorflow as tf
    tf.random.set_seed(args.seed)
    random.seed(args.seed)

    print(f"genero {args.synthetic} esempi sintetici...")
    x, y = make_synthetic(args.synthetic, args.seed)
    x_val, y_val = make_synthetic(max(2000, args.synthetic // 10), args.seed + 1)

    if args.real:
        xr, yr = load_real(args.real)
        if xr is None:
            print("nessun file reale trovato, uso solo dati sintetici")
        else:
            print(f"caricati {len(xr)} esempi reali (con specchiatura)")
            # i dati reali contano di più: ripetili
            rep = max(1, (len(x) // 4) // len(xr))
            x = np.concatenate([x] + [xr] * rep)
            y = np.concatenate([y] + [yr] * rep)
            n_val = max(1, len(xr) // 5)
            x_val = np.concatenate([x_val, xr[:n_val]])
            y_val = np.concatenate([y_val, yr[:n_val]])

    model = build_model()
    model.summary()
    model.compile(optimizer=tf.keras.optimizers.Adam(1e-3),
                  loss="sparse_categorical_crossentropy", metrics=["accuracy"])
    model.fit(x, y, validation_data=(x_val, y_val), epochs=args.epochs, batch_size=128,
              callbacks=[tf.keras.callbacks.ReduceLROnPlateau(patience=3, factor=0.5)], verbose=2)

    out_dir = pathlib.Path(args.out_dir)
    tfl = out_dir / "posture_cnn.tflite"
    cc = ROOT / "src" / "posture_model_data.cc"
    size = export(model, x, tfl, cc)
    acc, conf = evaluate_tflite(tfl, x_val[:2000], y_val[:2000])
    print(f"\nmodello int8: {size} byte -> {tfl} e {cc}")
    print(f"accuratezza int8 su validazione: {acc:.3f}")
    print("matrice di confusione (righe = vero, colonne = predetto):", CLASSES)
    print(conf)


if __name__ == "__main__":
    main()
