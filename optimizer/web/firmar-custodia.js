/* Xami /firmar — selector de credencial custodiada (demo en LAN).
 * Si la pagina corre con acceso al chip (mismo origin http), lee /device,
 * puebla el selector y cifra el auth (pass/TOTP) por ECIES hacia la pubkey
 * del chip. Si no hay chip, no hace nada y /firmar funciona como siempre. */
(function () {
  "use strict";
  const HKDF_INFO = "xami-match-v1";
  const CDN_FORGE = "https://cdnjs.cloudflare.com/ajax/libs/forge/1.3.1/forge.min.js";
  const CDN_EC    = "https://cdnjs.cloudflare.com/ajax/libs/elliptic/6.5.4/elliptic.min.js";

  const hex = u8 => [...u8].map(b => b.toString(16).padStart(2, "0")).join("");
  const u8ToBin = u => { let s = ""; for (let i = 0; i < u.length; i++) s += String.fromCharCode(u[i]); return s; };
  const binToU8 = b => Uint8Array.from(b, c => c.charCodeAt(0));
  const loadScript = src => new Promise((res, rej) => {
    const s = document.createElement("script"); s.src = src;
    s.onload = res; s.onerror = () => rej(new Error("no se pudo cargar " + src));
    document.head.appendChild(s); });
  function hmac256(keyBin, msgBin) { const h = forge.hmac.create(); h.start("sha256", keyBin); h.update(msgBin); return h.digest().getBytes(); }
  function hkdf32(ikmBin, saltBin, info) { const prk = hmac256(saltBin, ikmBin); return hmac256(prk, info + String.fromCharCode(1)).substring(0, 32); }

  /* ECIES — identica a custodia/app.js (crypto_match.ecies_encrypt). Retorna hex. */
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
    return hex(blob);
  }

  let chipPub = null;
  const creds = {};  /* slot -> {alias, sigType, mode, cert} */

  async function init() {
    let dev;
    try {
      const ctrl = new AbortController(); const t = setTimeout(() => ctrl.abort(), 2500);
      dev = await (await fetch(location.origin + "/device", { signal: ctrl.signal })).json();
      clearTimeout(t);
    } catch (e) { return; }                 /* sin chip -> /firmar normal */
    if (!dev || !dev.pubkey || !Array.isArray(dev.credentials) || !dev.credentials.length) return;
    try { await loadScript(CDN_FORGE); await loadScript(CDN_EC); } catch (e) { return; }
    chipPub = dev.pubkey;

    let opts = '<option value="">— clave del dispositivo (sin credencial) —</option>';
    dev.credentials.forEach(c => {
      creds[c.slot] = { alias: c.alias, sigType: c.sigType, mode: c.mode, cert: c.certPem };
      opts += '<option value="' + c.slot + '">' + (c.alias || ("slot " + c.slot)) +
              ' · ' + (c.sigType || "?") + ' · ' + (c.mode || "agente") + '</option>';
    });
    const box = document.createElement("div");
    box.style.cssText = "margin:12px 0;padding:12px;border:1px solid #2a3a5a;border-radius:8px";
    box.innerHTML =
      '<label style="display:block;margin-bottom:6px">Firmar con credencial custodiada</label>' +
      '<select id="xcred" style="width:100%;padding:6px">' + opts + '</select>' +
      '<div id="xauth" style="display:none;margin-top:8px">' +
      '<input id="xpass" type="password" placeholder="passphrase de la credencial" style="width:100%;padding:6px;margin-bottom:6px">' +
      '<input id="xtotp" type="text" inputmode="numeric" placeholder="codigo TOTP (6 digitos)" style="width:100%;padding:6px;display:none">' +
      '</div>';
    const anchor = document.getElementById("mode");
    if (anchor && anchor.parentNode) anchor.parentNode.insertBefore(box, anchor); else document.body.insertBefore(box, document.body.firstChild);
    const sel = document.getElementById("xcred");
    sel.addEventListener("change", () => {
      const s = sel.value;
      document.getElementById("xauth").style.display = s === "" ? "none" : "block";
      document.getElementById("xtotp").style.display = (s !== "" && creds[s] && creds[s].mode === "autorizacion") ? "block" : "none";
    });

    window.xamiAuthFields = async () => {
      const s = document.getElementById("xcred").value;
      if (s === "") return null;
      const c = creds[s]; if (!c) return null;
      const obj = { pass: (document.getElementById("xpass").value || "") };
      if (c.mode === "autorizacion") obj.totp = (document.getElementById("xtotp").value || "");
      const auth = eciesEncrypt(chipPub, new TextEncoder().encode(JSON.stringify(obj)));
      return { credential_id: String(s), credential_cert: c.cert || "", auth: auth };
    };
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init); else init();
})();
