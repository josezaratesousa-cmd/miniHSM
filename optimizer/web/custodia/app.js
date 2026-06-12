/* Xami — Custodia: pagina de ceremonia (app.js)
 * La sirve el CHIP (origen http en la LAN); carga forge+qrcode de CDN, abre el .p12,
 * cifra {secret,alias,cert,priv,pass} via ECIES hacia la pubkey del chip y POSTea /ceremony.
 * La logica vive en xami.run; el chip solo la arranca. Deuda tecnica: version offline/AP (B). */
(function () {
  "use strict";
  const API = "https://api.xami.run";
  const CHIP = location.origin;                 // el chip sirve esta pagina
  const HKDF_INFO = "xami-match-v1";
  const CDN_FORGE = "https://cdnjs.cloudflare.com/ajax/libs/forge/1.3.1/forge.min.js";
  const CDN_QR    = "https://cdnjs.cloudflare.com/ajax/libs/qrcodejs/1.0.0/qrcode.min.js";
  const enc = new TextEncoder();

  const hex = u8 => [...u8].map(b => b.toString(16).padStart(2, "0")).join("");
  const u8FromHex = h => { const a = new Uint8Array(h.length / 2);
    for (let i = 0; i < a.length; i++) a[i] = parseInt(h.substr(i * 2, 2), 16); return a; };
  const b64urlToU8 = s => { s = s.replace(/-/g, "+").replace(/_/g, "/"); while (s.length % 4) s += "=";
    const bin = atob(s); const a = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) a[i] = bin.charCodeAt(i); return a; };
  const loadScript = src => new Promise((res, rej) => {
    const s = document.createElement("script"); s.src = src;
    s.onload = res; s.onerror = () => rej(new Error("no se pudo cargar " + src));
    document.head.appendChild(s); });
  const sha256hex = async u8 => hex(new Uint8Array(await crypto.subtle.digest("SHA-256", u8)));

  /* ECIES — replica exacta de crypto_match.ecies_encrypt:
     blob = eph_pub(65) || iv(12) || ct||tag ; HKDF-SHA256 salt=32x00 info="xami-match-v1" ; AES-256-GCM */
  async function eciesEncrypt(pubHex, plaintextU8) {
    const chipPub = await crypto.subtle.importKey("raw", u8FromHex(pubHex),
      { name: "ECDH", namedCurve: "P-256" }, false, []);
    const eph = await crypto.subtle.generateKey({ name: "ECDH", namedCurve: "P-256" }, true, ["deriveBits"]);
    const ephPubRaw = new Uint8Array(await crypto.subtle.exportKey("raw", eph.publicKey));
    const shared = new Uint8Array(await crypto.subtle.deriveBits(
      { name: "ECDH", public: chipPub }, eph.privateKey, 256));
    const hk = await crypto.subtle.importKey("raw", shared, "HKDF", false, ["deriveBits"]);
    const keyBits = await crypto.subtle.deriveBits(
      { name: "HKDF", hash: "SHA-256", salt: new Uint8Array(32), info: enc.encode(HKDF_INFO) }, hk, 256);
    const aesKey = await crypto.subtle.importKey("raw", keyBits, "AES-GCM", false, ["encrypt"]);
    const iv = crypto.getRandomValues(new Uint8Array(12));
    const ctTag = new Uint8Array(await crypto.subtle.encrypt(
      { name: "AES-GCM", iv, tagLength: 128 }, aesKey, plaintextU8));
    const blob = new Uint8Array(65 + 12 + ctTag.length);
    blob.set(ephPubRaw, 0); blob.set(iv, 65); blob.set(ctTag, 77);
    return blob;
  }

  /* Abre el .p12 en el navegador (la contrasena del .p12 NUNCA sale de aqui).
     Devuelve { privHex (32B), certPem }. El chip solo firma P-256. */
  async function openP12(arrayBuf, p12pass) {
    const bytes = new Uint8Array(arrayBuf);
    let bin = ""; for (let i = 0; i < bytes.length; i++) bin += String.fromCharCode(bytes[i]);
    const asn1 = forge.asn1.fromDer(forge.util.createBuffer(bin, "binary"));
    let p12;
    try { p12 = forge.pkcs12.pkcs12FromAsn1(asn1, p12pass); }
    catch (e) { throw new Error("contrasena del .p12 incorrecta o archivo invalido"); }

    let cb = p12.getBags({ bagType: forge.pki.oids.certBag });
    cb = cb[forge.pki.oids.certBag] || [];
    if (!cb.length) throw new Error("el .p12 no contiene certificado");
    const certPem = forge.pki.certificateToPem(cb[0].cert).replace(/\r/g, "").trim();

    let kb = p12.getBags({ bagType: forge.pki.oids.pkcs8ShroudedKeyBag });
    kb = kb[forge.pki.oids.pkcs8ShroudedKeyBag] || [];
    if (!kb.length) { const k2 = p12.getBags({ bagType: forge.pki.oids.keyBag });
      kb = k2[forge.pki.oids.keyBag] || []; }
    if (!kb.length) throw new Error("el .p12 no contiene clave privada");

    const keyDer = forge.asn1.toDer(kb[0].asn1).getBytes();
    const keyU8 = Uint8Array.from(keyDer, c => c.charCodeAt(0));
    let jwk;
    try {
      const ck = await crypto.subtle.importKey("pkcs8", keyU8,
        { name: "ECDSA", namedCurve: "P-256" }, true, ["sign"]);
      jwk = await crypto.subtle.exportKey("jwk", ck);
    } catch (e) {
      throw new Error("la clave no es EC P-256 (el chip solo custodia claves P-256)");
    }
    const priv = b64urlToU8(jwk.d);
    if (priv.length !== 32) throw new Error("escalar privado invalido (" + priv.length + " bytes)");
    return { privHex: hex(priv), certPem };
  }

  /* ---- UI ---- */
  const $ = id => document.getElementById(id);
  function setStatus(msg, kind) {
    const el = $("cx-status"); if (!el) return;
    el.textContent = msg || ""; el.className = "cx-status" + (kind ? " cx-" + kind : "");
  }
  function injectUI() {
    const css = `
      .cx-wrap{max-width:560px;margin:24px auto;padding:0 16px;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;color:#1a1a2e}
      .cx-card{background:#fff;border:1px solid #e6e6ef;border-radius:16px;padding:22px;box-shadow:0 6px 24px rgba(30,30,60,.06);margin-bottom:16px}
      .cx-h{font-size:20px;font-weight:680;margin:0 0 2px}
      .cx-sub{font-size:13px;color:#6b6b85;margin:0 0 16px}
      .cx-row{margin:12px 0}
      .cx-lbl{display:block;font-size:13px;font-weight:600;margin-bottom:5px}
      .cx-in{width:100%;box-sizing:border-box;padding:10px 12px;border:1px solid #d8d8e6;border-radius:10px;font-size:14px}
      .cx-btn{width:100%;padding:12px;border:0;border-radius:10px;background:#3a36e0;color:#fff;font-size:15px;font-weight:600;cursor:pointer}
      .cx-btn:disabled{background:#bdbdd4;cursor:not-allowed}
      .cx-status{font-size:13px;margin-top:12px;min-height:18px}
      .cx-ok{color:#0a7d2c}.cx-err{color:#c8102e}.cx-warn{color:#b26a00}
      .cx-fp{font-family:ui-monospace,Menlo,monospace;font-size:12px;word-break:break-all;background:#f5f5fb;padding:8px 10px;border-radius:8px}
      .cx-hide{display:none}.cx-qr{display:flex;justify-content:center;margin:14px 0}
      .cx-mut{font-size:12px;color:#6b6b85}`;
    const st = document.createElement("style"); st.textContent = css; document.head.appendChild(st);
    document.body.innerHTML = `
      <div class="cx-wrap">
        <div class="cx-card">
          <p class="cx-h">Custodia Xami — Cargar credencial</p>
          <p class="cx-sub" id="cx-chip">conectando con el chip…</p>
          <div class="cx-row">
            <label class="cx-lbl" for="cx-alias">Nombre de la credencial (empleado)</label>
            <input class="cx-in" id="cx-alias" placeholder="ej. Maria Perez — Firmas RRHH" maxlength="40">
          </div>
          <button class="cx-btn" id="cx-start">Iniciar ceremonia</button>
        </div>
        <div class="cx-card cx-hide" id="cx-verify">
          <p class="cx-h" style="font-size:16px">Verificación de identidad del chip</p>
          <p class="cx-mut">El fingerprint local del chip debe coincidir con el que emitió xami.run:</p>
          <div class="cx-row"><span class="cx-mut">chip local:</span><div class="cx-fp" id="cx-fp-chip"></div></div>
          <div class="cx-row"><span class="cx-mut">xami.run:</span><div class="cx-fp" id="cx-fp-srv"></div></div>
          <p class="cx-status" id="cx-fp-res"></p>
        </div>
        <div class="cx-card cx-hide" id="cx-load">
          <p class="cx-h" style="font-size:16px">Cargar el .p12</p>
          <div class="cx-row"><label class="cx-lbl" for="cx-file">Archivo .p12 / .pfx</label>
            <input class="cx-in" id="cx-file" type="file" accept=".p12,.pfx"></div>
          <div class="cx-row"><label class="cx-lbl" for="cx-p12pass">Contraseña del .p12 (no sale del navegador)</label>
            <input class="cx-in" id="cx-p12pass" type="password" autocomplete="off"></div>
          <div class="cx-row"><label class="cx-lbl" for="cx-pass">Passphrase de custodia (la pedirás en cada firma)</label>
            <input class="cx-in" id="cx-pass" type="password" autocomplete="off"></div>
          <button class="cx-btn" id="cx-go">Cifrar y enviar al chip</button>
          <p class="cx-status" id="cx-status"></p>
        </div>
        <div class="cx-card cx-hide" id="cx-done">
          <p class="cx-h" style="font-size:16px">✓ Credencial custodiada</p>
          <p class="cx-mut" id="cx-slot"></p>
          <p class="cx-mut">Escanea este QR en Google Authenticator (TOTP por credencial):</p>
          <div class="cx-qr" id="cx-qr"></div>
        </div>
      </div>`;
  }

  /* ---- orquestacion ---- */
  let chipPub = null, chipDevId = null, cer = null;

  async function boot() {
    injectUI();
    try { await loadScript(CDN_FORGE); await loadScript(CDN_QR); }
    catch (e) { setStatus("No se pudieron cargar las librerias (forge/qr). El chip debe estar en tu WiFi con internet.", "err"); }
    try {
      const d = await (await fetch(CHIP + "/device")).json();
      chipPub = d.pubkey; chipDevId = d.deviceId;
      $("cx-chip").textContent = "chip " + chipDevId + " · " + (d.firmware || "") + " · " + (d.curve || "P-256");
    } catch (e) {
      $("cx-chip").textContent = "no se pudo contactar al chip (¿IP correcta? ¿misma WiFi?)";
    }
    $("cx-start").onclick = startCeremony;
    $("cx-go").onclick = doUpload;
  }

  async function startCeremony() {
    const alias = ($("cx-alias").value || "").trim();
    if (!alias) { alert("Pon un nombre para la credencial"); return; }
    if (!chipDevId) { alert("Aún no hay conexión con el chip"); return; }
    $("cx-start").disabled = true; $("cx-start").textContent = "iniciando…";
    try {
      const r = await fetch(API + "/v1/credentials/ceremony/start", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ deviceId: chipDevId, alias })
      });
      if (!r.ok) throw new Error("el server respondió " + r.status);
      cer = await r.json();
      const localFp = await sha256hex(u8FromHex(chipPub));
      $("cx-verify").classList.remove("cx-hide");
      $("cx-fp-chip").textContent = localFp;
      $("cx-fp-srv").textContent = cer.fingerprint;
      if (localFp === cer.fingerprint && chipPub === cer.pubkey) {
        $("cx-fp-res").textContent = "✓ Coinciden — el chip es legítimo.";
        $("cx-fp-res").className = "cx-status cx-ok";
        $("cx-load").classList.remove("cx-hide");
      } else {
        $("cx-fp-res").textContent = "✗ NO coinciden — posible impostor. No cargues la credencial.";
        $("cx-fp-res").className = "cx-status cx-err";
      }
    } catch (e) {
      $("cx-verify").classList.remove("cx-hide");
      $("cx-fp-res").textContent = "Error iniciando la ceremonia: " + e.message;
      $("cx-fp-res").className = "cx-status cx-err";
    } finally {
      $("cx-start").disabled = false; $("cx-start").textContent = "Iniciar ceremonia";
    }
  }

  async function postCeremonyWithRetry(blob) {
    for (let i = 0; i < 12; i++) {
      const r = await fetch(CHIP + "/ceremony", {
        method: "POST", headers: { "Content-Type": "application/octet-stream" }, body: blob });
      let j = null; try { j = await r.json(); } catch (_) {}
      if (j && j.ok) return j;
      if (j && j.error && (j.error.indexOf("armed") >= 0 || j.error.indexOf("secret") >= 0)) {
        setStatus("esperando que el chip reciba la autorización… (" + (i + 1) + ")", "warn");
        await new Promise(s => setTimeout(s, 3000)); continue;
      }
      return j || { ok: false, error: "sin respuesta del chip" };
    }
    return { ok: false, error: "el chip no recibió la autorización a tiempo" };
  }

  async function doUpload() {
    if (!cer) { setStatus("Primero inicia la ceremonia", "err"); return; }
    const f = $("cx-file").files[0];
    const p12pass = $("cx-p12pass").value;
    const pass = $("cx-pass").value;
    if (!f) { setStatus("Selecciona el archivo .p12", "err"); return; }
    if (!pass) { setStatus("Define una passphrase de custodia", "err"); return; }
    $("cx-go").disabled = true; setStatus("abriendo el .p12…");
    try {
      const buf = await f.arrayBuffer();
      const { privHex, certPem } = await openP12(buf, p12pass);
      setStatus("cifrando hacia el chip…");
      const payload = JSON.stringify({ secret: cer.secret, alias: cer.alias, cert: certPem, priv: privHex, pass });
      const blob = await eciesEncrypt(cer.pubkey, enc.encode(payload));
      setStatus("enviando al chip…");
      const res = await postCeremonyWithRetry(blob);
      if (res && res.ok) {
        $("cx-done").classList.remove("cx-hide");
        $("cx-slot").textContent = "Slot #" + res.slot + " · alias: " + cer.alias;
        if (res.otpauth) new QRCode($("cx-qr"), { text: res.otpauth, width: 200, height: 200 });
        setStatus("✓ Credencial custodiada", "ok"); $("cx-go").textContent = "Listo";
      } else {
        setStatus("El chip rechazó la carga: " + ((res && res.error) || "desconocido"), "err");
        $("cx-go").disabled = false;
      }
    } catch (e) {
      setStatus("Error: " + e.message, "err"); $("cx-go").disabled = false;
    }
  }

  if (document.readyState !== "loading") boot();
  else document.addEventListener("DOMContentLoaded", boot);
})();
