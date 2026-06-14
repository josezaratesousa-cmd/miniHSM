/* Xami — Enrolamiento de credencial (/enroll). Asistente de 3 pasos.
 * Servido por el CHIP (http, sin crypto.subtle) -> cripto en JS puro (forge+elliptic),
 * identica a crypto_match del server. La contrasena del .p12 no sale del navegador. */
(function () {
  "use strict";
  const API = "https://api.xami.run";
  const CHIP = location.origin;
  const HKDF_INFO = "xami-match-v1";
  const CDN_FORGE = "https://cdnjs.cloudflare.com/ajax/libs/forge/1.3.1/forge.min.js";
  const CDN_EC    = "https://cdnjs.cloudflare.com/ajax/libs/elliptic/6.5.4/elliptic.min.js";
  const CDN_QR    = "https://cdnjs.cloudflare.com/ajax/libs/qrcodejs/1.0.0/qrcode.min.js";

  const hex = u8 => [...u8].map(b => b.toString(16).padStart(2, "0")).join("");
  const u8FromHex = h => { const a = new Uint8Array(h.length / 2);
    for (let i = 0; i < a.length; i++) a[i] = parseInt(h.substr(i * 2, 2), 16); return a; };
  const u8ToBin = u => { let s = ""; for (let i = 0; i < u.length; i++) s += String.fromCharCode(u[i]); return s; };
  const binToU8 = b => Uint8Array.from(b, c => c.charCodeAt(0));
  const loadScript = src => new Promise((res, rej) => {
    const s = document.createElement("script"); s.src = src;
    s.onload = res; s.onerror = () => rej(new Error("no se pudo cargar " + src));
    document.head.appendChild(s); });

  /* ---- Cripto JS puro (forge + elliptic), funciona en http ---- */
  function sha256hex(u8) { const md = forge.md.sha256.create(); md.update(u8ToBin(u8)); return md.digest().toHex(); }
  function hmac256(keyBin, msgBin) { const h = forge.hmac.create(); h.start("sha256", keyBin); h.update(msgBin); return h.digest().getBytes(); }
  function hkdf32(ikmBin, saltBin, info) { const prk = hmac256(saltBin, ikmBin); return hmac256(prk, info + String.fromCharCode(1)).substring(0, 32); }

  /* ECIES — replica de crypto_match.ecies_encrypt: eph_pub65||iv12||ct||tag16 */
  function eciesEncrypt(pubHex, plaintextU8) {
    const ec = new elliptic.ec("p256");
    const chip = ec.keyFromPublic(pubHex, "hex");
    const eph = ec.genKeyPair();
    const ephPub = new Uint8Array(eph.getPublic().encode("array", false));
    const shared = new Uint8Array(eph.derive(chip.getPublic()).toArray("be", 32));
    const keyBin = hkdf32(u8ToBin(shared), u8ToBin(new Uint8Array(32)), HKDF_INFO);
    const iv = crypto.getRandomValues(new Uint8Array(12));
    const c = forge.cipher.createCipher("AES-GCM", keyBin);
    c.start({ iv: u8ToBin(iv), tagLength: 128 });
    c.update(forge.util.createBuffer(u8ToBin(plaintextU8)));
    c.finish();
    const ctTag = binToU8(c.output.getBytes() + c.mode.tag.getBytes());
    const blob = new Uint8Array(65 + 12 + ctTag.length);
    blob.set(ephPub, 0); blob.set(iv, 65); blob.set(ctTag, 77);
    return blob;
  }

  /* Abre el .p12 (la contrasena NO sale del navegador). */
  function openP12(arrayBuf, p12pass) {
    const bin = u8ToBin(new Uint8Array(arrayBuf));
    let p12;
    try { p12 = forge.pkcs12.pkcs12FromAsn1(forge.asn1.fromDer(forge.util.createBuffer(bin, "binary")), p12pass); }
    catch (e) { throw new Error("contrasena del .p12 incorrecta o archivo invalido"); }
    let cb = p12.getBags({ bagType: forge.pki.oids.certBag }); cb = cb[forge.pki.oids.certBag] || [];
    if (!cb.length) throw new Error("el .p12 no contiene certificado");
    let certAsn1 = cb[0].asn1;
    if (!certAsn1 && cb[0].cert) { try { certAsn1 = forge.pki.certificateToAsn1(cb[0].cert); } catch (e) {} }
    if (!certAsn1) throw new Error("no pude leer el certificado del .p12");
    const certDer = forge.asn1.toDer(certAsn1).getBytes();
    const b64 = forge.util.encode64(certDer);
    const certPem = "-----BEGIN CERTIFICATE-----\n" +
      b64.replace(/(.{64})/g, "$1\n").replace(/\n$/, "") + "\n-----END CERTIFICATE-----\n";
    let kb = p12.getBags({ bagType: forge.pki.oids.pkcs8ShroudedKeyBag }); kb = kb[forge.pki.oids.pkcs8ShroudedKeyBag] || [];
    if (!kb.length) { const k2 = p12.getBags({ bagType: forge.pki.oids.keyBag }); kb = k2[forge.pki.oids.keyBag] || []; }
    if (!kb.length) throw new Error("el .p12 no contiene clave privada");
    let pk8Der;
    if (kb[0].key) {
      try { pk8Der = forge.asn1.toDer(forge.pki.wrapRsaPrivateKey(forge.pki.privateKeyToAsn1(kb[0].key))).getBytes(); }
      catch (e) { throw new Error("no pude reconstruir la clave RSA del .p12"); }
    } else if (kb[0].asn1) {
      pk8Der = forge.asn1.toDer(kb[0].asn1).getBytes();
    } else { throw new Error("no pude leer la clave del .p12"); }
    return { privHex: hex(binToU8(pk8Der)), certPem };
  }

  /* ---- Estado ---- */
  const $ = id => document.getElementById(id);
  let chipPub = null, chipDevId = null, cer = null, step = 1, mode = "agente", chipOk = false;
  const PURPLE = "#534AB7";

  function setStatus(msg, kind) {
    const el = $("wz-status"); if (!el) return;
    el.textContent = msg || ""; el.style.display = msg ? "block" : "none";
    el.style.color = kind === "err" ? "#c0392b" : (kind === "ok" ? "#1a7f43" : "#555");
  }

  /* ---- UI ---- */
  function injectUI() {
    document.body.innerHTML = `
<style>
 *{box-sizing:border-box}
 body{margin:0;background:#eef1f6;font-family:-apple-system,Segoe UI,Roboto,sans-serif;color:#1a2233;
   min-height:100vh;display:flex;align-items:flex-start;justify-content:center;padding:22px 14px}
 .wz{background:#fff;border:1px solid #e6e9f0;border-radius:16px;max-width:470px;width:100%;
   padding:22px 24px;box-shadow:0 10px 40px rgba(20,30,60,.06)}
 .wz-top{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:4px}
 .wz-logo{height:30px;width:auto}
 .wz-vf{display:inline-flex;align-items:center;gap:4px;font-size:12px;color:#1a7f43;background:#e6f7ed;
   padding:3px 9px;border-radius:8px}
 .wz-vf.bad{color:#c0392b;background:#fdeaea}
 .wz-sub{margin:0 0 18px;font-size:12.5px;color:#8a93a6}
 .wz-steps{display:flex;align-items:center;gap:6px;margin-bottom:20px}
 .wz-dot{width:25px;height:25px;border-radius:50%;display:flex;align-items:center;justify-content:center;
   font-size:13px;font-weight:600;background:transparent;color:#aab2c2;border:1px solid #d7dce6;flex:0 0 auto}
 .wz-dot.on{background:${PURPLE};color:#fff;border:0}
 .wz-dot.done{background:#e6f7ed;color:#1a7f43;border:0}
 .wz-ln{flex:1;height:1px;background:#e0e4ec}
 .wz-slbl{font-size:12.5px;font-weight:500;color:#aab2c2}
 .wz-slbl.on{color:#1a2233}
 label{display:block;font-size:13px;font-weight:600;margin:14px 0 6px}
 .hint{font-size:11.5px;color:#8a93a6;margin:5px 0 0}
 input{width:100%;padding:12px 13px;border:1.5px solid #d7dce6;border-radius:10px;font-size:15px;outline:none}
 input:focus{border-color:${PURPLE}}
 .modes{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:6px}
 .mode{display:flex;flex-direction:column;align-items:flex-start;gap:3px;text-align:left;padding:12px;
   border-radius:11px;border:1px solid #d7dce6;background:#fff;cursor:pointer;font-family:inherit}
 .mode.on{border:2px solid ${PURPLE};background:#f3f2fd}
 .mode b{font-size:14px;font-weight:600}
 .mode small{font-size:11px;color:#6b7385}
 .msg{font-size:12.5px;line-height:1.55;color:#3a3a6a;background:#f3f2fd;border-radius:10px;padding:10px 12px;margin-top:12px}
 .nav{display:flex;gap:10px;margin-top:22px}
 .btn{flex:1;padding:13px;border:0;border-radius:10px;background:${PURPLE};color:#fff;font-size:15px;
   font-weight:600;cursor:pointer;font-family:inherit}
 .btn:disabled{opacity:.55;cursor:default}
 .btn.ghost{flex:0 0 auto;background:#eef1f6;color:#3a4253}
 .ga-seg{display:inline-flex;border:1px solid #d7dce6;border-radius:10px;overflow:hidden}
 .ga-seg button{padding:8px 14px;border:0;background:#fff;font-size:12.5px;cursor:pointer;font-family:inherit;color:#6b7385}
 .ga-seg button.on{background:#f3f2fd;color:${PURPLE};font-weight:600}
 .code{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:13px;background:#f1f3f8;padding:10px 12px;
   border-radius:9px;letter-spacing:1px;word-break:break-all;flex:1}
 #wz-qr{display:flex;justify-content:center;margin:4px 0}
 #wz-qr img,#wz-qr canvas{border-radius:8px}
 .hide{display:none}
 .ok-circle{width:46px;height:46px;border-radius:50%;background:#e6f7ed;display:inline-flex;align-items:center;
   justify-content:center;font-size:26px;color:#1a7f43}
</style>
<div class="wz">
  <div class="wz-top">
    <img src="/logo.png" class="wz-logo" alt="Xami">
    <span id="wz-vf" class="wz-vf hide"><span id="wz-vf-i">✓</span> <span id="wz-vf-t">chip verificado</span></span>
  </div>
  <p class="wz-sub" id="wz-chip">conectando con el chip…</p>

  <div class="wz-steps">
    <div class="wz-dot on" data-d="1">1</div><span class="wz-slbl on" data-l="1">Datos</span>
    <div class="wz-ln"></div>
    <div class="wz-dot" data-d="2">2</div><span class="wz-slbl" data-l="2">Certificado</span>
    <div class="wz-ln"></div>
    <div class="wz-dot" data-d="3">3</div><span class="wz-slbl" data-l="3">Listo</span>
  </div>

  <div class="pane" data-p="1">
    <label>Nombre de la credencial</label>
    <input id="wz-alias" type="text" placeholder="ej. Facturas Stamping" autocomplete="off">
    <label style="margin-top:16px">Como quieres que firme</label>
    <div class="modes">
      <button class="mode on" data-mode="agente"><span style="font-size:18px">🤖</span><b>Sola</b><small>firma automatica</small></button>
      <button class="mode" data-mode="auth"><span style="font-size:18px">🔒</span><b>Con tu permiso</b><small>clave + Google Authenticator</small></button>
    </div>
    <div class="msg" id="wz-mmsg"></div>
  </div>
  <div class="pane hide" data-p="2">
    <label>Archivo del certificado <span style="color:#8a93a6;font-weight:400">(.p12 o .pfx)</span></label>
    <input id="wz-file" type="file" accept=".p12,.pfx">
    <label style="margin-top:14px">Contrasena del archivo</label>
    <input id="wz-p12pass" type="password" autocomplete="off">
    <p class="hint">No sale de este navegador.</p>
    <div id="wz-passrow" class="hide">
      <label style="margin-top:14px">Tu clave de custodia</label>
      <input id="wz-pass" type="password" autocomplete="off">
      <p class="hint">Te la pediremos cada vez que firmes.</p>
    </div>
  </div>

  <div class="pane hide" data-p="3">
    <div style="text-align:center;padding:8px 0 4px">
      <div class="ok-circle">✓</div>
      <p style="margin:10px 0 0;font-size:15px;font-weight:600">Credencial guardada en el chip</p>
      <p id="wz-done-sub" style="margin:4px 0 0;font-size:13px;color:#6b7385"></p>
    </div>
    <div id="wz-ga" class="hide" style="margin-top:16px">
      <p style="margin:0 0 9px;font-size:13px;font-weight:600;text-align:center">Activa Google Authenticator</p>
      <div style="display:flex;justify-content:center;margin-bottom:12px">
        <div class="ga-seg">
          <button id="wz-ga-qr-t" class="on">Escanear QR</button>
          <button id="wz-ga-code-t">Ingresar codigo</button>
        </div>
      </div>
      <div id="wz-ga-qr-v">
        <div id="wz-qr"></div>
        <p style="margin:9px 0 0;font-size:11.5px;color:#6b7385;text-align:center">Estas en la PC? Abre Google Authenticator en tu telefono y escanea.</p>
      </div>
      <div id="wz-ga-code-v" class="hide">
        <p style="margin:0 0 7px;font-size:11.5px;color:#6b7385;text-align:center">En el mismo telefono? Abre Google Authenticator, toca <b>+ &rarr; Ingresar clave de configuracion</b> y pega:</p>
        <div style="display:flex;gap:8px;align-items:center">
          <span id="wz-secret" class="code"></span>
          <button id="wz-copy" class="btn ghost" style="padding:9px 12px;font-size:12.5px">Copiar</button>
        </div>
      </div>
    </div>
  </div>

  <p id="wz-status" style="display:none;font-size:12.5px;text-align:center;margin:14px 0 0"></p>
  <div class="nav">
    <button id="wz-back" class="btn ghost hide">Atras</button>
    <button id="wz-next" class="btn">Siguiente</button>
  </div>
</div>`;
  }

  /* ---- Navegacion ---- */
  const MMSG = {
    agente: "El chip crea su propia llave y la guarda con xami.run. Firma sola, sin pedirte nada ni usar tu telefono.",
    auth: "Tu apruebas cada firma con tu clave y un codigo de Google Authenticator. Al final te mostramos un QR (o un codigo para copiar) para activarlo."
  };
  function paintSteps() {
    [1,2,3].forEach(n => {
      const dot = document.querySelector(`.wz-dot[data-d="${n}"]`);
      const lbl = document.querySelector(`.wz-slbl[data-l="${n}"]`);
      dot.className = "wz-dot" + (n < step ? " done" : n === step ? " on" : "");
      dot.textContent = n < step ? "✓" : n;
      lbl.className = "wz-slbl" + (n === step ? " on" : "");
    });
  }
  function showPane() {
    document.querySelectorAll(".pane").forEach(p => p.classList.toggle("hide", +p.dataset.p !== step));
    $("wz-back").classList.toggle("hide", step === 1 || step === 3);
    const nx = $("wz-next");
    nx.textContent = step === 1 ? "Siguiente" : step === 2 ? "Guardar en el chip" : "Hecho";
    nx.disabled = false;
    paintSteps();
  }
  function pickMode(m) {
    mode = m;
    document.querySelectorAll(".mode").forEach(b => b.classList.toggle("on", b.dataset.mode === m));
    $("wz-mmsg").textContent = MMSG[m];
  }
  function gaTab(which) {
    const qr = which === "qr";
    $("wz-ga-qr-t").classList.toggle("on", qr);
    $("wz-ga-code-t").classList.toggle("on", !qr);
    $("wz-ga-qr-v").classList.toggle("hide", !qr);
    $("wz-ga-code-v").classList.toggle("hide", qr);
  }

  /* ---- Verificacion del chip (discreta) ---- */
  function showVerify(ok) {
    chipOk = ok;
    const el = $("wz-vf"); el.classList.remove("hide");
    el.classList.toggle("bad", !ok);
    $("wz-vf-i").textContent = ok ? "✓" : "⚠";
    $("wz-vf-t").textContent = ok ? "chip verificado" : "chip NO verificado";
  }

  /* ---- Pasos ---- */
  async function armCeremony() {
    const alias = ($("wz-alias").value || "").trim();
    const r = await fetch(API + "/v1/credentials/ceremony/start", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ deviceId: chipDevId, alias })
    });
    if (!r.ok) throw new Error("el server respondio " + r.status);
    cer = await r.json();
    const localFp = sha256hex(u8FromHex(chipPub));
    showVerify(localFp === cer.fingerprint && chipPub === cer.pubkey);
  }

  async function postCeremonyWithRetry(blob) {
    for (let i = 0; i < 14; i++) {
      const r = await fetch(CHIP + "/ceremony", {
        method: "POST", headers: { "Content-Type": "application/octet-stream" }, body: blob });
      let j = null; try { j = await r.json(); } catch (_) {}
      if (j && j.ok) return j;
      if (j && j.error && (j.error.indexOf("armed") >= 0 || j.error.indexOf("secret") >= 0)) {
        setStatus("esperando que el chip reciba la autorizacion… (" + (i + 1) + ")");
        await new Promise(s => setTimeout(s, 3000)); continue;
      }
      return j || { ok: false, error: "sin respuesta del chip" };
    }
    return { ok: false, error: "el chip no recibio la autorizacion a tiempo" };
  }

  async function doGuardar() {
    const f = $("wz-file").files[0];
    const p12pass = $("wz-p12pass").value;
    const pass = $("wz-pass").value;
    if (!f) { setStatus("Selecciona el archivo .p12", "err"); return false; }
    if (mode === "auth" && !pass) { setStatus("Define tu clave de custodia", "err"); return false; }
    $("wz-next").disabled = true; $("wz-back").disabled = true;
    setStatus("abriendo el .p12…");
    try {
      const buf = await f.arrayBuffer();
      const { privHex, certPem } = openP12(buf, p12pass);
      setStatus("cifrando hacia el chip…");
      const body = { secret: cer.secret, alias: cer.alias, cert: certPem, priv: privHex, mode: mode === "auth" ? 1 : 0 };
      if (mode === "auth") body.pass = pass;
      const blob = eciesEncrypt(cer.pubkey, new TextEncoder().encode(JSON.stringify(body)));
      setStatus("enviando al chip…");
      const res = await postCeremonyWithRetry(blob);
      if (res && res.ok) { setStatus(""); finishStep(res); return true; }
      setStatus("El chip rechazo la carga: " + ((res && res.error) || "desconocido"), "err");
      $("wz-next").disabled = false; $("wz-back").disabled = false; return false;
    } catch (e) {
      setStatus("Error: " + e.message, "err");
      $("wz-next").disabled = false; $("wz-back").disabled = false; return false;
    }
  }

  function finishStep(res) {
    step = 3; showPane();
    if (mode === "auth" && res.otpauth) {
      $("wz-done-sub").textContent = "Solo falta activar tu Google Authenticator.";
      $("wz-ga").classList.remove("hide");
      $("wz-qr").innerHTML = "";
      new QRCode($("wz-qr"), { text: res.otpauth, width: 180, height: 180 });
      const m = /[?&]secret=([^&]+)/i.exec(res.otpauth);
      $("wz-secret").textContent = m ? m[1].replace(/(.{4})/g, "$1 ").trim() : res.otpauth;
      gaTab("qr");
    } else {
      $("wz-done-sub").textContent = "El chip ya puede firmar solo.";
      $("wz-ga").classList.add("hide");
    }
  }

  async function onNext() {
    if (step === 1) {
      const alias = ($("wz-alias").value || "").trim();
      if (!alias) { setStatus("Pon un nombre para la credencial", "err"); return; }
      if (!chipDevId) { setStatus("Aun no hay conexion con el chip", "err"); return; }
      $("wz-next").disabled = true; setStatus("preparando el chip…");
      try { await armCeremony(); }
      catch (e) { setStatus("No se pudo iniciar: " + e.message, "err"); $("wz-next").disabled = false; return; }
      if (!chipOk) { setStatus("El chip no coincide con xami.run. No continues.", "err"); $("wz-next").disabled = false; return; }
      setStatus(""); step = 2; showPane();
    } else if (step === 2) {
      await doGuardar();
    } else {
      location.reload();  /* "Hecho": reinicia el wizard para enrolar otra credencial */
    }
  }

  async function boot() {
    injectUI();
    $("wz-next").onclick = onNext;
    $("wz-back").onclick = () => { if (step > 1) { step--; setStatus(""); showPane(); } };
    document.querySelectorAll(".mode").forEach(b => b.onclick = () => {
      pickMode(b.dataset.mode);
      $("wz-passrow").classList.toggle("hide", b.dataset.mode !== "auth");
    });
    $("wz-ga-qr-t").onclick = () => gaTab("qr");
    $("wz-ga-code-t").onclick = () => gaTab("code");
    $("wz-copy").onclick = function () {
      const t = $("wz-secret").textContent.replace(/\s+/g, "");
      try { navigator.clipboard.writeText(t); } catch (e) {}
      this.textContent = "Copiado";
    };
    pickMode("agente");
    try { await loadScript(CDN_FORGE); await loadScript(CDN_EC); await loadScript(CDN_QR); }
    catch (e) { setStatus("No se pudieron cargar las librerias. El chip debe estar en tu WiFi con internet.", "err"); }
    try {
      const d = await (await fetch(CHIP + "/device")).json();
      chipPub = d.pubkey; chipDevId = (String(d.deviceId || "").match(/[0-9a-f]{16}/i) || [d.deviceId])[0];
      const fw = (d.firmware && d.firmware.version) ? d.firmware.version : (d.firmware || "?");
      $("wz-chip").textContent = "chip " + chipDevId + " · " + fw + " · " + (d.curve || "P-256");
    } catch (e) {
      $("wz-chip").textContent = "no se pudo contactar al chip (¿IP correcta? ¿misma WiFi?)";
    }
  }

  if (document.readyState !== "loading") boot();
  else document.addEventListener("DOMContentLoaded", boot);
})();
