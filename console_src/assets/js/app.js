'use strict';
const API = '/app/api.php';
const ICONS={
 check:'<path d="M5 12l5 5L20 7"/>',
 drag:'<path d="M12 2v20M5 9l-3 3 3 3M19 9l3 3-3 3M9 5l3-3 3 3M9 19l3 3 3-3"/>',
 chevronDown:'<path d="M6 9l6 6 6-6"/>',
 doc:'<path d="M6 2h7l5 5v15H6z"/><path d="M13 2v5h5"/>',
 alignLeft:'<path d="M4 6h10M4 12h16M4 18h10"/>',
 alignCenter:'<path d="M7 6h10M4 12h16M7 18h10"/>',
 alignRight:'<path d="M10 6h10M4 12h16M10 18h10"/>',
 alignTop:'<path d="M4 5h16M8 9v10M12 9v10M16 9v10"/>',
 alignMiddle:'<path d="M4 12h16M8 6v4M8 14v4M16 6v4M16 14v4"/>',
 alignBottom:'<path d="M4 19h16M8 5v10M12 5v10M16 5v10"/>',

 x:'<path d="M6 6l12 12M18 6L6 18"/>',
 'file-x':'<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><path d="M10 14l4 4M14 14l-4 4"/>',
 'file-check':'<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><path d="M9 15l2 2 3-3"/>',
 file:'<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/>',
 send:'<path d="M22 2L11 13"/><path d="M22 2l-7 20-4-9-9-4z"/>',
 copy:'<rect x="9" y="9" width="11" height="11" rx="2"/><path d="M5 15V5a2 2 0 0 1 2-2h8"/>',
 eye:'<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7-10-7-10-7z"/><circle cx="12" cy="12" r="3"/>',
 save:'<path d="M5 3h12l4 4v12a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z"/><path d="M7 3v6h8V3M7 21v-6h10v6"/>',
 id:'<rect x="3" y="5" width="18" height="14" rx="2"/><circle cx="9" cy="11" r="2"/><path d="M14 9h4M14 13h4M6 16h7"/>',
 upload:'<path d="M12 15V4M8 8l4-4 4 4"/><path d="M4 17v2a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-2"/>',
 device:'<rect x="7" y="3" width="10" height="18" rx="2"/><path d="M11 18h2"/>',
 image:'<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><path d="M21 15l-5-5L5 21"/>',
 trash:'<path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"/>',
 download:'<path d="M12 4v11M8 11l4 4 4-4"/><path d="M4 19h16"/>',
 info:'<circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 7.5v.5"/>',
 settings:'<circle cx="12" cy="12" r="3"/><path d="M19.4 13a7.9 7.9 0 0 0 0-2l2-1.5-2-3.4-2.4 1a8 8 0 0 0-1.7-1l-.4-2.6h-4l-.4 2.6a8 8 0 0 0-1.7 1l-2.4-1-2 3.4L4.6 11a7.9 7.9 0 0 0 0 2l-2 1.5 2 3.4 2.4-1a8 8 0 0 0 1.7 1l.4 2.6h4l.4-2.6a8 8 0 0 0 1.7-1l2.4 1 2-3.4z"/>',
 chart:'<path d="M4 20V10M10 20V4M16 20v-7M22 20H2"/>',
 pencil:'<path d="M4 20h4L18 10l-4-4L4 16z"/><path d="M13 7l4 4"/>',
 stamp:'<path d="M5 21h14M7 17h10v-2a5 5 0 0 0-3-4.6V7a2 2 0 1 0-4 0v3.4A5 5 0 0 0 7 15z"/>', chev:'<path d="M6 9l6 6 6-6"/>', layout:'<rect x="3" y="3" width="18" height="18" rx="2"/><path d="M3 9h18M9 21V9"/>',
 plus:'<path d="M12 5v14M5 12h14"/>'
};
function svg(n,s){s=s||16;return '<svg width="'+s+'" height="'+s+'" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" style="vertical-align:-3px">'+(ICONS[n]||ICONS.file)+'</svg>';}

const SUN_SVG='<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M5 5l1.5 1.5M17.5 17.5L19 19M19 5l-1.5 1.5M6.5 17.5L5 19"/></svg>';
const MOON_SVG='<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M21 12.8A8 8 0 1 1 11.2 3 6 6 0 0 0 21 12.8z"/></svg>';

/* ---------- Tema claro/oscuro (recordado en localStorage) ---------- */
(function initTheme(){
  var t = localStorage.getItem('xami_theme') || 'light';
  if (t === 'dark') document.documentElement.setAttribute('data-theme','dark');
})();
function applyThemeIcon(){
  var dark = document.documentElement.getAttribute('data-theme') === 'dark';
  var btn = document.getElementById('themeToggle');
  if (btn) btn.innerHTML = dark ? SUN_SVG : MOON_SVG;
}
document.addEventListener('DOMContentLoaded', function(){
  applyThemeIcon();
  var btn = document.getElementById('themeToggle');
  if (btn) btn.addEventListener('click', function(){
    var dark = document.documentElement.getAttribute('data-theme') === 'dark';
    if (dark){ document.documentElement.removeAttribute('data-theme'); localStorage.setItem('xami_theme','light'); }
    else { document.documentElement.setAttribute('data-theme','dark'); localStorage.setItem('xami_theme','dark'); }
    applyThemeIcon();
  });
});

/* ---------- Sidebar: colapsar + redimensionar (localStorage) ---------- */
const sidebar = document.getElementById('sidebar');
const resizer = document.getElementById('resizer');
const toggle  = document.getElementById('toggleSidebar');

(function initSidebar(){
  const w = localStorage.getItem('xami_sidebar_width');
  if (w) document.documentElement.style.setProperty('--sidebar-w', w + 'px');
  if (localStorage.getItem('xami_sidebar_collapsed') === '1') sidebar.classList.add('collapsed');
})();

toggle.addEventListener('click', () => {
  sidebar.classList.toggle('collapsed');
  localStorage.setItem('xami_sidebar_collapsed', sidebar.classList.contains('collapsed') ? '1' : '0');
});

let resizing = false;
resizer.addEventListener('mousedown', e => { resizing = true; e.preventDefault(); document.body.style.userSelect='none'; });
window.addEventListener('mousemove', e => {
  if (!resizing) return;
  let w = Math.min(Math.max(e.clientX, 170), 380);
  document.documentElement.style.setProperty('--sidebar-w', w + 'px');
});
window.addEventListener('mouseup', () => {
  if (!resizing) return;
  resizing = false; document.body.style.userSelect='';
  const w = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--sidebar-w'));
  localStorage.setItem('xami_sidebar_width', w);
});

/* ---------- Navegación entre vistas ---------- */
const main = document.getElementById('main');
document.querySelectorAll('.nav-item').forEach(it => {
  it.addEventListener('click', e => {
    e.preventDefault();
    document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
    it.classList.add('active');
    renderView(it.dataset.view);
  });
});

/* ---------- Helpers ---------- */
async function apiGet(action, params={}){
  const q = new URLSearchParams({action, ...params});
  const r = await fetch(`${API}?${q}`, {credentials:'same-origin'});
  if (!r.ok) throw new Error('HTTP '+r.status);
  return r.json();
}
async function apiPost(action, body){
  const r = await fetch(API+'?action='+action, {method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});
  if(!r.ok) throw new Error('HTTP '+r.status);
  return r.json();
}
function fileURL(designId){ return '/app/file.php?type=signature&id='+designId+'&t='+Date.now(); }
function fmtSize(b){ if(b==null)return '—'; if(b<1024)return b+' B'; if(b<1048576)return Math.round(b/1024)+' KB'; return (b/1048576).toFixed(1)+' MB'; }
function fmtDate(s){ if(!s)return '—'; const d=new Date(s.replace(' ','T')); return d.toLocaleDateString('es',{day:'2-digit',month:'short'})+', '+d.toLocaleTimeString('es',{hour:'2-digit',minute:'2-digit'}); }
function esc(s){ const d=document.createElement('div'); d.textContent=s==null?'':s; return d.innerHTML; }

/* ---------- Vistas ---------- */
function renderView(view){
  if (['pendientes','firmados','rechazados'].includes(view)) return renderBandeja(view);
  if (view === 'dispositivos') return renderDispositivos();
  if (view === 'preferencias') return renderPreferencias();
  if (view === 'consumo')      return renderSimple('Mi consumo','Próximamente: saldo y consumo de firmas.');
}

const BANDEJA_CFG = {
  pendientes:{titulo:'Pendientes de firma', fechaCol:'Solicitado', fechaKey:'created_at', dateClass:''},
  firmados:  {titulo:'Firmados',            fechaCol:'Firmado',    fechaKey:'signed_at',  dateClass:'date-ok'},
  rechazados:{titulo:'Rechazados',          fechaCol:'Rechazado',  fechaKey:'closed_at',  dateClass:'date-bad'},
};

async function renderBandeja(view){
  const cfg = BANDEJA_CFG[view];
  main.innerHTML = `<div class="main-loading">Cargando…</div>`;
  let data;
  try { data = await apiGet('list', {bandeja:view}); }
  catch(e){ main.innerHTML = `<div class="empty">Error al cargar: ${esc(e.message)}</div>`; return; }

  const rows = data.items.map(it => rowHTML(it, view, cfg)).join('');
  const nuevoBtn = view==='pendientes' ? `<button class="btn sm" id="btnNuevo">${svg("pencil")} Nuevo</button>` : '';
  main.innerHTML = `
    <div class="view-head">
      <div class="view-title">${cfg.titulo} <span class="badge-n">${data.items.length}</span></div>
      ${nuevoBtn}
    </div>
    <div class="list">
      <div class="list-head"><span></span><span>Documento</span><span class="tright">Tamaño</span><span class="tright">Págs.</span><span>${cfg.fechaCol}</span><span></span></div>
      ${rows || `<div class="empty">No hay documentos en esta bandeja.</div>`}
    </div>`;

  if (view==='pendientes'){ const b=document.getElementById('btnNuevo'); if(b) b.onclick=openNuevo; }
  document.querySelectorAll('.list-row').forEach(r => r.onclick = () => openDetalle(r.dataset.id));
  refreshCounts();
}

function rowHTML(it, view, cfg){
  const fecha = it[cfg.fechaKey];
  let action = '';
  if (view==='pendientes'){
    action = it.estado==='entregado'
      ? `<span class="badge cola">En cola</span>`
      : `<button class="btn sm" onclick="event.stopPropagation();signDoc(${it.id})">Firmar</button><button class="btn sm danger" onclick="event.stopPropagation();rejectDoc(${it.id})">${svg("x")}</button>`;
  } else if (view==='firmados'){
    action = `<button class="btn sm ghost" onclick="event.stopPropagation();dl(${it.id})">${svg("download")}</button>`;
  } else {
    action = `<span class="badge" title="${esc(it.motivo_rechazo||'')}">motivo</span>`;
  }
  const icon = view==='firmados'?svg("file-check",18):(view==='rechazados'?svg("file-x",18):svg("file",18));
  return `<div class="list-row" data-id="${it.id}">
    <span class="ico-doc">${icon}</span>
    <div class="lr-doc"><div class="lr-name">${esc(it.filename)}</div><div class="lr-origin">${esc(it.origen||'')}</div></div>
    <span class="lr-meta lr-size tright">${fmtSize(it.size_bytes)}</span>
    <span class="lr-meta lr-pages tright">${it.pages??'—'}</span>
    <span class="lr-meta lr-date ${cfg.dateClass}">${fmtDate(fecha)}</span>
    <span class="lr-actions">${action}</span>
  </div>`;
}

async function refreshCounts(){
  try{ const c = await apiGet('counts');
    document.getElementById('c-pendientes').textContent = c.pendientes;
    document.getElementById('c-firmados').textContent   = c.firmados;
    document.getElementById('c-rechazados').textContent = c.rechazados;
  }catch(e){}
}

function renderSimple(t,m){ main.innerHTML = `<div class="view-head"><div class="view-title">${t}</div></div><div class="empty">${m}</div>`; }

/* ---------- Drawer ---------- */
const drawer = document.getElementById('drawer');
const drawerInner = document.getElementById('drawerInner');
const overlay = document.getElementById('overlay');
function openDrawer(html){ drawerInner.innerHTML = html; drawer.classList.add('show'); overlay.classList.add('show'); }
function closeDrawer(){ drawer.classList.remove('show'); overlay.classList.remove('show'); }
overlay.addEventListener('click', closeDrawer);

async function openDetalle(id){
  openDrawer(`<div class="main-loading">Cargando…</div>`);
  let d;
  try{ d = await apiGet('detail',{id}); }
  catch(e){ drawerInner.innerHTML=`<div class="empty">Error: ${esc(e.message)}</div>`; return; }
  const it = d.item, ev = d.events||[];
  drawerInner.innerHTML = `
    <div class="drawer-head"><span class="dh-title">${esc(it.filename)}</span><button class="dh-close" onclick="closeDrawer()">&times;</button></div>
    <div class="tabs">
      <div class="tab active" data-tab="doc">Documento</div>
      <div class="tab" data-tab="traza">Trazabilidad <span class="tab-n">${ev.length}</span></div>
    </div>
    <div id="tab-doc">
      <div class="preview-box"><span style="font-size:38px">${svg("file",18)}</span><span>Vista previa del PDF</span><span style="font-size:11px">${it.pages||'—'} página(s)</span></div>
      <div style="display:flex;gap:8px;margin-top:14px">${detailActions(it)}</div>
    </div>
    <div id="tab-traza" style="display:none">${timelineHTML(ev)}</div>`;
  drawerInner.querySelectorAll('.tab').forEach(t => t.onclick = () => {
    drawerInner.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));
    t.classList.add('active');
    drawerInner.querySelector('#tab-doc').style.display   = t.dataset.tab==='doc'?'block':'none';
    drawerInner.querySelector('#tab-traza').style.display = t.dataset.tab==='traza'?'block':'none';
  });
}

function detailActions(it){
  if (it.estado==='pendiente') return `<button class="btn" onclick="signDoc(${it.id})">${svg("pencil")} Firmar</button><button class="btn danger" onclick="rejectDoc(${it.id})">${svg("x")} Rechazar</button>`;
  if (it.estado==='firmado')   return `<button class="btn ghost" onclick="dl(${it.id})">${svg("download")} Descargar firmado</button>`;
  if (it.estado==='rechazado') return `<div class="tl-date">Motivo: ${esc(it.motivo_rechazo||'—')}</div>`;
  return `<span class="badge cola">En cola</span>`;
}

function timelineHTML(ev){
  const meta = {enviado:[svg("send"),'Enviado a firmar',''],abierto:[svg("eye"),'Abierto / revisado',''],procesado:[svg("settings",20),'Procesado por el dispositivo',''],firmado:[svg("check"),'Firmado','ok'],rechazado:[svg("x"),'Rechazado','bad']};
  if(!ev.length) return `<div class="empty">Sin eventos aún.</div>`;
  return ev.map((e,i)=>{
    const m = meta[e.tipo]||['&#8226;',e.tipo,''];
    const last = i===ev.length-1;
    return `<div class="tl-item ${m[2]}">
      <div class="tl-dot"><span class="dot">${m[0]}</span>${last?'':'<span class="line"></span>'}</div>
      <div class="tl-body"><div class="tl-title">${m[1]}</div><div class="tl-date">${fmtDate(e.created_at)}</div><div class="tl-actor">${esc(e.actor||'')}</div></div>
    </div>`;
  }).join('');
}

/* ---------- Nueva firma (placeholder de form; encolado en fase siguiente) ---------- */
let _nf = null;

async function openNuevo(){
  _nf = { dev:null, pdf:null, design:null, designs:[], accOpen:false,
          align_h:'right', align_v:'bottom', adv:false, fx:'', fy:'' };
  // cargar disenos (predeterminado primero)
  let dz=[]; try{ const r=await apiGet('designs'); dz=r.items||[]; }catch(e){}
  dz.sort((a,b)=>(b.es_default?1:0)-(a.es_default?1:0));
  _nf.designs=dz;
  _nf.design = dz.find(d=>d.es_default) || dz[0] || null;
  let devs=[]; try{ const r=await apiGet('devices'); devs=r.items||[]; }catch(e){}
  _nf.devs=devs;
  drawer.classList.remove('wide');
  openDrawer(nuevoHTML());
  bindNuevo();
}

function nuevoHTML(){
  const devOpts = (_nf.devs&&_nf.devs.length)
    ? _nf.devs.map(d=>`<option value="${esc(d.device_id)}">${esc(d.alias||d.device_id)} · ${esc(d.device_id)}</option>`).join('')
    : `<option>Sin dispositivos</option>`;
  return `
  <div class="drawer-head"><span class="dh-title">${svg("pencil")} Nueva firma</span><button class="dh-close" onclick="closeNuevo()">&times;</button></div>
  <div class="nf">
    <div class="fld"><label>Firmar con</label><select id="nf-dev">${devOpts}</select></div>

    <div class="fld"><label>Documento</label>
      <div id="nf-dropwrap">${_nf.pdf ? nfPdfCard() : `<div class="dropzone" id="nf-drop">${svg("upload",22)}<br>Arrastra el PDF o haz clic</div>`}</div>
      <input type="file" id="nf-pdffile" accept="application/pdf" hidden>
    </div>

    <div class="fld"><label>Diseño de firma</label>
      <div id="nf-acc">${nfAccordion()}</div>
    </div>

    <div class="fld"><label>Página</label>
      <select id="nf-page">
        <option value="last">Última página</option>
        <option value="last-1">Última − 1</option>
        <option value="first">Primera página</option>
        <option value="n">Número específico…</option>
      </select>
      <input type="number" id="nf-pagenum" min="1" value="1" style="display:none;margin-top:6px;width:90px" placeholder="N°">
    </div>

    <div class="fld">
      <div class="nf-poshead"><label style="margin:0">¿Dónde va el sello?</label><span class="nf-advtoggle" id="nf-advtoggle" onclick="nfToggleAdv()">${svg("settings",13)} Avanzado</span></div>
      <button type="button" class="nf-dragbtn" onclick="nfOpenDragSim()">${svg("drag",15)||svg("settings",15)} Posicionar sobre el documento</button>
      <div id="nf-possimple">
        <div class="nf-alignrow"><span>Horizontal</span>
          <button type="button" class="nf-al ${_nf.align_h==='left'?'on':''}" data-h="left">${svg("alignLeft",15)}</button>
          <button type="button" class="nf-al ${_nf.align_h==='center'?'on':''}" data-h="center">${svg("alignCenter",15)}</button>
          <button type="button" class="nf-al ${_nf.align_h==='right'?'on':''}" data-h="right">${svg("alignRight",15)}</button>
        </div>
        <div class="nf-alignrow"><span>Vertical</span>
          <button type="button" class="nf-al ${_nf.align_v==='top'?'on':''}" data-v="top">${svg("alignTop",15)}</button>
          <button type="button" class="nf-al ${_nf.align_v==='middle'?'on':''}" data-v="middle">${svg("alignMiddle",15)}</button>
          <button type="button" class="nf-al ${_nf.align_v==='bottom'?'on':''}" data-v="bottom">${svg("alignBottom",15)}</button>
        </div>
      </div>
      <div id="nf-posadv" style="display:none">
        <div class="nf-fxrow"><span>x</span><input type="text" id="nf-fx" placeholder="ancho − 220" value="${esc(_nf.fx)}"></div>
        <div class="nf-fxrow"><span>y</span><input type="text" id="nf-fy" placeholder="40" value="${esc(_nf.fy)}"></div>
        <div class="nf-fxhint">Variables: <code>ancho</code>, <code>alto</code> del PDF. Ej: <code>ancho/2</code> centra.</div>
        <div id="nf-fxresolved" class="nf-fxresolved"></div>
      </div>
    </div>

    <div style="display:flex;gap:8px;margin-top:6px">
      <button class="btn" id="nf-send" onclick="nfSend()">${svg("send")} Enviar a firmar</button>
      <button class="btn ghost" onclick="closeNuevo()">Cancelar</button>
    </div>
  </div>`;
}

function nfPdfCard(){
  const p=_nf.pdf;
  return `<div class="nf-pdfcard">
    ${svg("doc",24)}
    <div class="nf-pdfmeta"><div class="nf-pdfname">${esc(p.filename)}</div>
    <div class="nf-pdfsub">${fmtSize(p.size)} · ${p.pages} pág · ${Math.round(p.width)}×${Math.round(p.height)} pt</div></div>
    <button type="button" class="nf-pdfx" onclick="nfClearPdf()">${svg("x",15)}</button>
  </div>`;
}

function nfAccordion(){
  if(!_nf.designs.length) return `<div class="nf-nodesign">No tienes diseños. Crea uno en Preferencias.</div>`;
  const sel=_nf.design;
  if(!_nf.accOpen){
    return `<div class="nf-accbox">
      <div class="nf-accrow head" onclick="nfToggleAcc()">
        ${nfMiniPrev(sel)}
        <div class="nf-acclabel">${esc(sel.nombre)} ${sel.es_default?'<span class="nf-badge">predeterminado</span>':''}</div>
        ${svg("chevronDown",16)}
      </div></div>`;
  }
  const rows=_nf.designs.map(d=>`
    <div class="nf-accrow ${d.id===sel.id?'on':''}" onclick="nfPickDesign(${d.id})">
      ${nfMiniPrev(d)}
      <div class="nf-acclabel">${esc(d.nombre)} ${d.es_default?'<span class="nf-badge">predeterminado</span>':''}</div>
      ${d.id===sel.id?svg("check",15):''}
    </div>`).join('');
  return `<div class="nf-accbox open">${rows}</div>`;
}

function nfMiniPrev(d){
  const img = d.image_path ? fileURL(d.id) : null;
  return `<div class="nf-mini">${stampHTML(d.params||{}, img)}</div>`;
}

function bindNuevo(){
  const file=document.getElementById('nf-pdffile');
  const drop=document.getElementById('nf-drop');
  if(drop&&file){
    drop.onclick=()=>file.click();
    file.onchange=()=>{ if(file.files[0]) nfUploadPdf(file.files[0]); };
    ['dragover','dragleave','drop'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.toggle('over',ev==='dragover');}));
    drop.addEventListener('drop',e=>{ const f=e.dataTransfer.files[0]; if(f) nfUploadPdf(f); });
  }
  const pageSel=document.getElementById('nf-page');
  if(pageSel) pageSel.onchange=()=>{ document.getElementById('nf-pagenum').style.display = pageSel.value==='n'?'block':'none'; };
  document.querySelectorAll('.nf-al').forEach(b=>b.onclick=()=>{
    if(b.dataset.h){ _nf.align_h=b.dataset.h; document.querySelectorAll('.nf-al[data-h]').forEach(x=>x.classList.toggle('on',x===b)); }
    if(b.dataset.v){ _nf.align_v=b.dataset.v; document.querySelectorAll('.nf-al[data-v]').forEach(x=>x.classList.toggle('on',x===b)); }
  });
  const fx=document.getElementById('nf-fx'), fy=document.getElementById('nf-fy');
  if(fx) fx.addEventListener('input',nfResolveFormulas);
  if(fy) fy.addEventListener('input',nfResolveFormulas);
}

async function nfUploadPdf(f){
  const wrap=document.getElementById('nf-dropwrap');
  wrap.innerHTML=`<div class="dropzone">Subiendo y midiendo…</div>`;
  try{
    const fd=new FormData(); fd.append('pdf', f);
    const r=await fetch(API+'?action=pdf_upload',{method:'POST',credentials:'same-origin',body:fd});
    const j=await r.json();
    if(j&&j.ok){ _nf.pdf=j; wrap.innerHTML=nfPdfCard(); nfResolveFormulas(); }
    else { wrap.innerHTML=`<div class="dropzone" id="nf-drop">${svg("upload",22)}<br>Error. Reintenta</div>`; bindNuevo(); }
  }catch(e){ wrap.innerHTML=`<div class="dropzone" id="nf-drop">${svg("upload",22)}<br>Error: ${esc(e.message)}</div>`; bindNuevo(); }
}
function nfClearPdf(){ _nf.pdf=null; document.getElementById('nf-dropwrap').innerHTML=`<div class="dropzone" id="nf-drop">${svg("upload",22)}<br>Arrastra el PDF o haz clic</div>`; bindNuevo(); }

function nfToggleAcc(){ _nf.accOpen=!_nf.accOpen; document.getElementById('nf-acc').innerHTML=nfAccordion(); }
function nfPickDesign(id){ _nf.design=_nf.designs.find(d=>d.id===id); _nf.accOpen=false; document.getElementById('nf-acc').innerHTML=nfAccordion(); }

function nfToggleAdv(){
  _nf.adv=!_nf.adv;
  document.getElementById('nf-possimple').style.display=_nf.adv?'none':'block';
  document.getElementById('nf-posadv').style.display=_nf.adv?'block':'none';
  document.getElementById('nf-advtoggle').classList.toggle('on',_nf.adv);
  if(_nf.adv) nfResolveFormulas();
}

// resolutor seguro de formulas: solo ancho, alto, numeros, + - * / ( )
function nfEval(expr, W, H){
  if(!expr||!expr.trim()) return null;
  let e=expr.toLowerCase().replace(/ancho/g,'('+W+')').replace(/alto/g,'('+H+')');
  e=e.replace(/[−–]/g,'-'); // normalizar guiones
  if(!/^[\d\s+\-*/().]+$/.test(e)) return NaN; // caracteres no permitidos
  try{ const v=Function('"use strict";return('+e+')')(); return (typeof v==='number'&&isFinite(v))?v:NaN; }catch(_){ return NaN; }
}
function nfResolveFormulas(){
  const box=document.getElementById('nf-fxresolved'); if(!box) return;
  if(!_nf.pdf){ box.innerHTML=`<span class="muted2">Sube un PDF para estimar.</span>`; return; }
  const W=_nf.pdf.width, H=_nf.pdf.height;
  const fx=document.getElementById('nf-fx').value, fy=document.getElementById('nf-fy').value;
  _nf.fx=fx; _nf.fy=fy;
  const x=nfEval(fx,W,H), y=nfEval(fy,W,H);
  if(x===null&&y===null){ box.innerHTML=''; return; }
  const bad = Number.isNaN(x)||Number.isNaN(y);
  if(bad){ box.innerHTML=`<span class="nf-bad">${svg("x",12)} Fórmula inválida</span>`; return; }
  box.innerHTML=`<span class="nf-ok">${svg("check",12)} En este PDF (${Math.round(W)}×${Math.round(H)}): x=${Math.round(x)}, y=${Math.round(y)}</span>`;
}

function nfResolvePosition(){
  if(!_nf.pdf||!_nf.design) return null;
  const W=_nf.pdf.width, H=_nf.pdf.height;
  const p=_nf.design.params||{};
  const sw=p.stamp_w||400, sh=p.stamp_h||120;
  let x, y;
  if(_nf.adv){
    x=nfEval(document.getElementById('nf-fx').value, W, H);
    y=nfEval(document.getElementById('nf-fy').value, W, H);
    if(x===null||y===null||Number.isNaN(x)||Number.isNaN(y)) return null;
  } else {
    const m=36;
    if(_nf.align_h==='left') x=m; else if(_nf.align_h==='center') x=(W-sw)/2; else x=W-sw-m;
    if(_nf.align_v==='bottom') y=m; else if(_nf.align_v==='middle') y=(H-sh)/2; else y=H-sh-m;
  }
  x=Math.max(0,Math.min(x, W-sw));
  y=Math.max(0,Math.min(y, H-sh));
  return {x1:Math.round(x), y1:Math.round(y), x2:Math.round(x+sw), y2:Math.round(y+sh)};
}

async function nfSend(){
  if(!_nf.pdf){ alert('Primero sube un documento PDF.'); return; }
  if(!_nf.design){ alert('Elige un diseño de firma.'); return; }
  const pos=nfResolvePosition();
  if(!pos){ alert('Revisa la posición: la fórmula no es válida.'); return; }
  const dev=document.getElementById('nf-dev').value;
  const pageRef=document.getElementById('nf-page').value;
  const pageNum=+(document.getElementById('nf-pagenum').value||1);
  const box=`${pos.x1},${pos.y1},${pos.x2},${pos.y2}`;

  const btn=document.getElementById('nf-send');
  btn.disabled=true; btn.style.opacity='0.6'; const orig=btn.innerHTML; btn.innerHTML='Enviando…';
  try{
    const r=await apiPost('sign_send',{
      pdf_path:_nf.pdf.path, design_id:_nf.design.id, device_id:dev,
      page_ref:pageRef, page_num:pageNum, box, filename:_nf.pdf.filename
    });
    if(!r||!r.ok){ alert('No se pudo enviar: '+(r&&r.error||'error')); btn.disabled=false; btn.style.opacity=''; btn.innerHTML=orig; return; }
    closeNuevo();
    renderView('pendientes');
    refreshCounts();
    nfPoll(r.request_id, 0);
  }catch(e){
    alert('Error al enviar: '+e.message); btn.disabled=false; btn.style.opacity=''; btn.innerHTML=orig;
  }
}

async function nfPoll(rid, tries){
  if(tries>80) return;
  try{
    const r=await apiGet('sign_status',{request_id:rid});
    if(r&&r.final){ refreshCounts(); renderView('pendientes'); return; }
  }catch(e){}
  setTimeout(()=>nfPoll(rid, tries+1), 2500);
}


// ===== Simulador drag-and-drop de posición sobre el PDF (pdf.js bajo demanda) =====
const PDFJS_URL='https://cdnjs.cloudflare.com/ajax/libs/pdf.js/3.11.174/pdf.min.js';
const PDFJS_WORKER='https://cdnjs.cloudflare.com/ajax/libs/pdf.js/3.11.174/pdf.worker.min.js';
let _pdfjsLoading=null;
function loadPdfJs(){
  if(window.pdfjsLib) return Promise.resolve(window.pdfjsLib);
  if(_pdfjsLoading) return _pdfjsLoading;
  _pdfjsLoading=new Promise((res,rej)=>{
    const s=document.createElement('script'); s.src=PDFJS_URL;
    s.onload=()=>{ try{ window.pdfjsLib.GlobalWorkerOptions.workerSrc=PDFJS_WORKER; }catch(e){} res(window.pdfjsLib); };
    s.onerror=()=>rej(new Error('no se pudo cargar pdf.js'));
    document.head.appendChild(s);
  });
  return _pdfjsLoading;
}

// resuelve la pagina seleccionada (last/last-1/first/n) a numero real 1-indexed
function nfResolvePageNum(){
  const ref=document.getElementById('nf-page').value;
  const N=_nf.pdf.pages||1;
  if(ref==='last') return N;
  if(ref==='last-1') return Math.max(1,N-1);
  if(ref==='first') return 1;
  if(ref==='n') return Math.min(Math.max(1,+(document.getElementById('nf-pagenum').value||1)),N);
  return 1;
}

async function nfOpenDragSim(){
  if(!_nf.pdf){ alert('Primero sube un documento PDF.'); return; }
  if(!_nf.design){ alert('Elige un diseño de firma.'); return; }
  // overlay modal
  let ov=document.getElementById('dragsim-ov');
  if(ov) ov.remove();
  ov=document.createElement('div'); ov.id='dragsim-ov'; ov.className='dragsim-ov';
  ov.innerHTML=`
    <div class="dragsim-box">
      <div class="dragsim-head">
        <span>${svg("drag",16)||''} Arrastra el sello a su lugar</span>
        <button class="dragsim-x" onclick="nfCloseDragSim()">&times;</button>
      </div>
      <div class="dragsim-body">
        <div class="dragsim-stage" id="dragsim-stage">
          <canvas id="dragsim-canvas"></canvas>
          <div id="dragsim-stamp" class="dragsim-stamp">${stampHTML(_nf.design.params||{}, _nf.design.image_path?fileURL(_nf.design.id):null)}</div>
        </div>
      </div>
      <div class="dragsim-foot">
        <div class="dragsim-coords" id="dragsim-coords">x=–, y=–</div>
        <div class="dragsim-acts">
          <button class="btn ghost" onclick="nfCloseDragSim()">Cancelar</button>
          <button class="btn" onclick="nfApplyDragSim()">${svg("check",15)} Usar esta posición</button>
        </div>
      </div>
    </div>`;
  document.body.appendChild(ov);
  try{
    await loadPdfJs();
    await nfRenderPdfPage();
  }catch(e){ alert('No se pudo abrir el PDF: '+e.message); nfCloseDragSim(); }
}

async function nfRenderPdfPage(){
  const url=API.replace('api.php','file.php')+'?type=pdf_preview&path='+encodeURIComponent(_nf.pdf.path);
  const pdf=await window.pdfjsLib.getDocument(url).promise;
  const pageNum=nfResolvePageNum();
  const page=await pdf.getPage(pageNum);
  // escalar para caber en el stage
  const stage=document.getElementById('dragsim-stage');
  const maxW=Math.min(stage.clientWidth||520, 520);
  const vp1=page.getViewport({scale:1});
  const scale=maxW/vp1.width;
  const vp=page.getViewport({scale});
  const canvas=document.getElementById('dragsim-canvas');
  canvas.width=vp.width; canvas.height=vp.height;
  await page.render({canvasContext:canvas.getContext('2d'),viewport:vp}).promise;
  // guardar factores para convertir px->pt del PDF
  _nf._sim={ scale, pdfW:vp1.width, pdfH:vp1.height, cw:vp.width, ch:vp.height };
  // dimensionar el sello segun su tamaño real escalado
  const p=_nf.design.params||{};
  const sw=(p.stamp_w||400)*scale, sh=(p.stamp_h||120)*scale;
  const stamp=document.getElementById('dragsim-stamp');
  stamp.style.width=sw+'px'; stamp.style.height=sh+'px';
  // posicion inicial: segun fórmula/alineación actual o esquina inf-der
  let pos=nfResolvePosition();
  if(!pos){ const m=36; pos={x1:_nf._sim.pdfW-(p.stamp_w||400)-m, y1:m}; pos.x2=pos.x1+(p.stamp_w||400); pos.y2=pos.y1+(p.stamp_h||120); }
  nfPlaceStamp(pos.x1, pos.y1);
  nfBindDrag();
}

// coloca el sello en coords PDF (origen abajo-izq) -> px en canvas (origen arriba-izq)
function nfPlaceStamp(xPt, yPt){
  const s=_nf._sim, p=_nf.design.params||{};
  const sw=p.stamp_w||400, sh=p.stamp_h||120;
  const leftPx=xPt*s.scale;
  const topPx=(s.pdfH-yPt-sh)*s.scale; // invertir Y
  const stamp=document.getElementById('dragsim-stamp');
  stamp.style.left=leftPx+'px'; stamp.style.top=topPx+'px';
  _nf._sim.curX=Math.round(xPt); _nf._sim.curY=Math.round(yPt);
  const c=document.getElementById('dragsim-coords');
  if(c) c.textContent=`x=${Math.round(xPt)}, y=${Math.round(yPt)} pt  ·  página ${nfResolvePageNum()}`;
}

function nfBindDrag(){
  const stamp=document.getElementById('dragsim-stamp');
  const stage=document.getElementById('dragsim-stage');
  let drag=false, ox=0, oy=0;
  stamp.onmousedown=e=>{
    drag=true; const r=stamp.getBoundingClientRect();
    ox=e.clientX-r.left; oy=e.clientY-r.top; stamp.style.cursor='grabbing'; e.preventDefault();
  };
  window.addEventListener('mousemove', _nf._dragMove=e=>{
    if(!drag) return;
    const sr=stage.getBoundingClientRect(), s=_nf._sim, p=_nf.design.params||{};
    let leftPx=e.clientX-sr.left-ox, topPx=e.clientY-sr.top-oy;
    const sw=(p.stamp_w||400)*s.scale, sh=(p.stamp_h||120)*s.scale;
    leftPx=Math.max(0,Math.min(leftPx, s.cw-sw));
    topPx=Math.max(0,Math.min(topPx, s.ch-sh));
    // px -> pt PDF (invertir Y)
    const xPt=leftPx/s.scale;
    const yPt=s.pdfH - (topPx/s.scale) - (p.stamp_h||120);
    nfPlaceStamp(xPt, yPt);
  });
  window.addEventListener('mouseup', _nf._dragUp=()=>{ if(drag){drag=false; stamp.style.cursor='grab';} });
}

function nfApplyDragSim(){
  // guarda la posición como FÓRMULA relativa (para servir a otros PDFs) y pasa a modo avanzado
  const s=_nf._sim, p=_nf.design.params||{};
  const x=s.curX, y=s.curY;
  // fórmula: si está cerca del borde derecho, usar ancho - margen; si no, valor directo
  const distRight=s.pdfW-(x+(p.stamp_w||400));
  const fx = (distRight < x) ? `ancho - ${Math.round(s.pdfW - x)}` : `${x}`;
  const distTop=s.pdfH-(y+(p.stamp_h||120));
  const fy = (distTop < y) ? `alto - ${Math.round(s.pdfH - y)}` : `${y}`;
  _nf.fx=fx; _nf.fy=fy; _nf.adv=true;
  // refrescar la UI de posición a modo avanzado con las fórmulas
  document.getElementById('nf-possimple').style.display='none';
  document.getElementById('nf-posadv').style.display='block';
  document.getElementById('nf-advtoggle').classList.add('on');
  document.getElementById('nf-fx').value=fx;
  document.getElementById('nf-fy').value=fy;
  nfResolveFormulas();
  nfCloseDragSim();
}

function nfCloseDragSim(){
  if(_nf&&_nf._dragMove) window.removeEventListener('mousemove',_nf._dragMove);
  if(_nf&&_nf._dragUp) window.removeEventListener('mouseup',_nf._dragUp);
  const ov=document.getElementById('dragsim-ov'); if(ov) ov.remove();
}

function closeNuevo(){ drawer.classList.remove('wide'); closeDrawer(); _nf=null; }

/* ---------- Dispositivos ---------- */
async function renderDispositivos(){
  main.innerHTML = `<div class="main-loading">Cargando…</div>`;
  let d; try{ d = await apiGet('devices'); }catch(e){ main.innerHTML=`<div class="empty">Error: ${esc(e.message)}</div>`; return; }
  const rows = d.items.map(x=>`<div class="list-row"><span class="ico-doc">${svg("device",20)}</span>
    <div class="lr-doc"><div class="lr-name">${esc(x.alias||x.device_id)}</div><div class="lr-origin">${esc(x.device_id)} · ${esc(x.firmware||'')}</div></div>
    <span></span><span></span><span class="lr-meta">${esc(x.estado)}</span>
    <span class="lr-actions"><span class="badge ${x.online?'':'cola'}">${x.online?'En línea':'Dormido'}</span></span></div>`).join('');
  main.innerHTML = `<div class="view-head"><div class="view-title">Mis dispositivos <span class="badge-n">${d.items.length}</span></div></div>
    <div class="list"><div class="list-head"><span></span><span>Dispositivo</span><span></span><span></span><span>Estado</span><span></span></div>${rows||'<div class="empty">Sin dispositivos.</div>'}</div>`;
}

/* ---------- Acciones (placeholders de la siguiente subfase) ---------- */
function signDoc(id){ alert('Firmar #'+id+': encolado real en la siguiente subfase.'); }
function rejectDoc(id){ alert('Rechazar #'+id+': en la siguiente subfase.'); }
function dl(id){ alert('Descargar #'+id+': en la siguiente subfase.'); }

/* ---------- Arranque ---------- */
renderView('pendientes');
refreshCounts();

/* ====================== PREFERENCIAS ====================== */
function renderPreferencias(){
  main.innerHTML = `
    <div class="view-head"><div class="view-title">Mis preferencias</div></div>
    <p style="color:var(--muted);font-size:13px;margin-bottom:16px">Configura una vez y reutiliza en cada firma.</p>
    <div class="pref-cards">
      <div class="pref-card" id="card-disenos">
        <div class="pc-ico" style="background:rgba(77,184,255,.12);color:var(--accent)">${svg("pencil")}</div>
        <div class="pc-title">Diseño de firmas</div>
        <div class="pc-desc">Apariencia del sello visible: datos, imagen, posición.</div>
        <div class="pc-foot"><span class="badge" id="pc-count">…</span><span style="margin-left:auto">&rarr;</span></div>
      </div>
      <div class="pref-card" style="opacity:.6">
        <div class="pc-ico">${svg("id",20)}</div>
        <div class="pc-title">Datos de firmante</div>
        <div class="pc-desc">Nombre, razón, lugar y contacto por defecto.</div>
        <div class="pc-foot"><span style="color:var(--muted);font-size:12px">Próximamente</span></div>
      </div>
      <div class="pref-card" style="opacity:.6">
        <div class="pc-ico">${svg("settings",20)}</div>
        <div class="pc-title">Sellado de tiempo</div>
        <div class="pc-desc">Definido por tu organización (TSA del tenant).</div>
        <div class="pc-foot"><span class="badge">Solo lectura</span></div>
      </div>
    </div>`;
  document.getElementById('card-disenos').onclick = renderDisenos;
  apiGet('designs').then(d=>{ const c=document.getElementById('pc-count'); if(c) c.textContent = d.items.length+' diseños'; }).catch(()=>{});
}

/* ---------- Galería de diseños ---------- */
async function renderDisenos(){
  main.innerHTML = `<div class="main-loading">Cargando…</div>`;
  let d; try{ d=await apiGet('designs'); }catch(e){ main.innerHTML=`<div class="empty">Error: ${esc(e.message)}</div>`; return; }
  const cards = d.items.map(x=>designCardHTML(x)).join('');
  main.innerHTML = `
    <div class="view-head">
      <div class="view-title"><span style="cursor:pointer" onclick="renderPreferencias()">&larr;</span> Diseño de firmas <span class="badge-n">${d.items.length}</span></div>
    </div>
    <p style="color:var(--muted);font-size:13px;margin-bottom:16px">Plantillas reutilizables del sello. Elige una al firmar.</p>
    <div class="design-grid">
      ${cards}
      <div class="design-new" onclick="openEditor(0)"><span style="font-size:26px">+</span><span>Nuevo diseño</span></div>
    </div>`;
}

function stampHTML(p, imagePath, imageURL){
  const lines = stampLines(p);
  const hasText = lines.length>0;
  const bw = p.border ? (p.border_width||1) : 0;
  const border = bw ? `${bw}px solid var(--accent)` : '1px solid #e3e9f2';
  const imgSrc = imageURL || imagePath || '';
  const bg = `<div style="position:absolute;inset:0;background:${p.fill_color||'#FFFFFF'};opacity:${p.fill_opacity??0}"></div>`;
  const txt = `<div class="sps-txt" style="color:#000">${lines.map(l=>`<div>${esc(l)}</div>`).join('')}</div>`;
  let inner;
  if(imgSrc && !hasText){
    inner = `<div style="position:absolute;inset:0;background:url('${imgSrc}') center/cover no-repeat"></div>`;
  } else if(imgSrc && p.image_mode==='background'){
    inner = `<div style="position:absolute;inset:0;background:url('${imgSrc}') center/contain no-repeat"></div><div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center">${txt}</div>`;
  } else if(imgSrc && p.image_mode==='left'){
    const iw=parseInt(p.image_width)||40;
    inner = `<div style="position:absolute;inset:0;display:flex;align-items:center;gap:5px;padding:4px"><div style="width:${iw}%;height:100%;flex-shrink:0;background:url('${imgSrc}') left center/contain no-repeat"></div><div style="flex:1;min-width:0">${txt}</div></div>`;
  } else if(hasText){
    inner = `<div style="position:absolute;inset:0;display:flex;align-items:center;padding:4px">${txt}</div>`;
  } else {
    inner = `<div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:#b9c4d4;font-size:9px">Sin contenido</div>`;
  }
  return `<div class="dprev-stamp" style="border:${border};position:relative;overflow:hidden">${bg}${inner}</div>`;
}
function designCardHTML(x){
  const p = x.params||{};
  const prev = p.visible
    ? `<div class="dprev">${stampHTML(p, x.image_path?fileURL(x.id):null)}</div>`
    : `<div class="dprev"><div class="dprev-none">Sin sello visible</div></div>`;
  return `<div class="design-card">
    ${prev}
    <div class="dc-body">
      <div class="dc-head"><span class="dc-name">${esc(x.nombre)}</span>${x.es_default?'<span class="badge-def">Predeterminado</span>':''}</div>
      <div class="dc-actions">
        <button class="btn sm ghost" onclick="openEditor(${x.id})">${svg("pencil")} Editar</button>
        <button class="btn sm ghost" onclick="dupDesign(${x.id})" title="Duplicar">${svg("copy")}</button>
        <button class="btn sm ghost" onclick="delDesign(${x.id})" title="Eliminar">${svg("trash")}</button>
      </div>
    </div>
  </div>`;
}

async function dupDesign(id){
  const d = await apiGet('design_get',{id});
  await apiPost('design_save',{nombre:d.nombre+' (copia)', params:d.params, es_default:0});
  renderDisenos();
}
async function delDesign(id){
  if(!confirm('¿Eliminar este diseño?')) return;
  await apiPost('design_delete',{id});
  renderDisenos();
}

/* ====================== EDITOR DE DISEÑO (drawer ancho) ====================== */
let _editDesign = null;

async function openEditor(id){
  let d;
  if (id){ d = await apiGet('design_get',{id}); }
  else { d = { id:0, nombre:'Nuevo diseño', es_default:0, params: defaultParams() }; }
  _editDesign = d;
  // si el diseno tiene imagen guardada, usarla en el preview (no es blob, es ruta del servidor)
  if(d.image_path && d.id){ _editDesign._imgURL = fileURL(d.id); }
  drawer.classList.add('wide');
  openDrawer(editorHTML(d));
  bindEditor();
  updatePreview();
}

function defaultParams(){
  return {
    firmante:{value:(window.__userName||'Usuario'),ask:false},
    reason:{value:'',ask:false}, location:{value:'',ask:false}, contact:{value:'',ask:false},
    mode:'approval', visible:true, page:1,
    box:{x1:40,y1:40,x2:440,y2:160},
    stamp_source:'custom',
    stamp_lines:[(window.__userName||'Usuario')],
    add_date:true,
    stamp_w:400, stamp_h:120,
    image_mode:'left', image_width:'40%',
    image_opacity:1.0, text_opacity:1.0, fill_opacity:0.0, fill_color:'#FFFFFF',
    border:false, border_width:2
  };
}

function editorHTML(d){
  const p=d.params;
  const lines=(p.stamp_lines&&p.stamp_lines.length)?p.stamp_lines:[''];
  const addDate = p.add_date!==false;
  return `
  <div class="drawer-head"><span class="dh-title">${svg("pencil")} ${d.id?'Editar':'Nueva'} firma</span><button class="dh-close" onclick="closeEditor()">&times;</button></div>
  <div class="editor">

    <div class="ed-form">
      <div class="fld"><label>Nombre de esta firma</label><input type="text" id="ed-nombre" value="${esc(d.nombre)}" placeholder="Mi firma de contratos"></div>

      <div class="fld"><label>Tu logo o firma <span class="muted2">(opcional)</span></label>
        <div class="dropzone" id="ed-imgdrop">${d.image_path?'Imagen cargada · clic para cambiar':svg("upload",20)+'<br>Arrastra una imagen'}</div>
        <input type="file" id="ed-imgfile" accept="image/*" hidden>
      </div>

      <div class="fld"><label>¿Qué quieres que diga?</label>
        <div id="ed-lines-wrap">${lines.map((l,i)=>lineRow(l,i)).join('')}</div>
        <button type="button" class="ed-addline" onclick="addLine()">${svg("plus",15)} Agregar línea</button>
        <label class="ed-datecheck"><input type="checkbox" id="ed-adddate" ${addDate?'checked':''}> Añadir la fecha automáticamente</label>
      </div>
    </div>

    <div class="ed-right">
      <div class="ed-prev-label">${svg("eye")} Así se verá tu firma</div>
      <div class="ed-sheet" id="ed-sheet">
        <div class="sheet-lines"><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span><span></span></div>
        <div class="sheet-stamp" id="ed-stamp"></div>
        <button type="button" class="stamp-gear" id="ed-gear" onclick="toggleGear()" aria-label="Ajustar el sello">${svg("settings",15)}</button>
        <div class="gear-pop" id="ed-gearpop" style="display:none">
          <div class="gear-title"><span>${svg("settings",14)} Ajustar el sello</span><button type="button" class="gear-close" onclick="toggleGear()" aria-label="Cerrar">${svg("x",14)}</button></div>
          <div class="rng"><label>Ancho</label><input type="range" id="ed-w" min="120" max="520" value="${p.stamp_w||400}"><span id="ed-wv">${p.stamp_w||400}</span></div>
          <div class="rng"><label>Alto</label><input type="range" id="ed-h" min="60" max="300" value="${p.stamp_h||120}"><span id="ed-hv">${p.stamp_h||120}</span></div>
          <div class="rng"><label>Tam. imagen</label><input type="range" id="ed-iw" min="20" max="80" value="${parseInt(p.image_width)||40}"><span id="ed-iwv">${parseInt(p.image_width)||40}%</span></div>
          <div class="rng"><label>Op. fondo</label><input type="range" id="ed-bopa" min="0" max="100" value="${Math.round((p.fill_opacity??0)*100)}"><span id="ed-bopav">${Math.round((p.fill_opacity??0)*100)}%</span></div>
          <div class="rng"><label>Color fondo</label><input type="color" id="ed-fcolor" value="${p.fill_color||'#FFFFFF'}" style="width:100%;height:24px;padding:0;border:1px solid var(--line);border-radius:6px;cursor:pointer"><span></span></div>
          <div class="gear-row"><span class="gr-label">Disposición</span><select id="ed-imgmode"><option value="left"${p.image_mode==='left'?' selected':''}>Imagen al lado</option><option value="background"${p.image_mode==='background'?' selected':''}>Imagen de fondo</option></select></div>
          <div class="gear-row"><label class="gr-check"><input type="checkbox" id="ed-border" ${p.border?'checked':''}> Borde</label>
            <span id="ed-bwrow" style="${p.border?'':'display:none'};display:inline-flex;align-items:center;gap:5px"><input type="number" id="ed-bw" min="0" max="10" value="${p.border_width||2}" style="width:50px"><span class="muted2">px</span></span>
          </div>
        </div>
      </div>
      <p class="ed-pos-note">${svg("info",14)} La página y posición se eligen al firmar cada documento.</p>
      <div class="ed-prev-actions">
        <button class="btn" onclick="saveDesign()">${svg("save")} Guardar firma</button>
        <button class="btn ghost" onclick="closeEditor()">Cancelar</button>
        <label class="ed-defcheck2"><input type="checkbox" id="ed-default" ${d.es_default?'checked':''}> Predeterminada</label>
      </div>
    </div>
  </div>`;
}

function lineRow(val,i){
  return `<div class="ed-line"><input type="text" class="ed-lineinput" value="${esc(val)}" placeholder="Escribe una línea…"><button type="button" class="ed-linedel" onclick="delLine(this)" aria-label="Eliminar línea">${svg("x",14)}</button></div>`;
}
function addLine(){
  const w=document.getElementById('ed-lines-wrap');
  const div=document.createElement('div');
  div.innerHTML=lineRow('',0);
  const node=div.firstChild;
  w.appendChild(node);
  node.querySelector('input').addEventListener('input',updatePreview);
  node.querySelector('input').focus();
  updatePreview();
}
function delLine(btn){
  const wrap=document.getElementById('ed-lines-wrap');
  if(wrap.querySelectorAll('.ed-line').length<=1){ btn.previousElementSibling.value=''; }
  else { btn.closest('.ed-line').remove(); }
  updatePreview();
}
function toggleGear(){
  const pop=document.getElementById('ed-gearpop');
  const gear=document.getElementById('ed-gear');
  const show = pop.style.display==='none';
  if(show){
    // sacar el popover al body para que el overflow:hidden del sheet no lo recorte (FIX2)
    if(pop.parentElement.id!=='__body_pop'){ document.body.appendChild(pop); }
    pop.style.position='fixed';
    pop.style.zIndex='9999';
    pop.style.display='block';
    const r=gear.getBoundingClientRect();
    const pw=248, ph=pop.offsetHeight||260;
    // abrir hacia la izquierda y arriba del engranaje; si no cabe arriba, hacia abajo
    let left=r.left-pw-8; if(left<8) left=r.right+8;
    let top=r.bottom-ph; if(top<8) top=8;
    pop.style.left=left+'px'; pop.style.top=top+'px'; pop.style.right='auto'; pop.style.bottom='auto';
    makeDraggable(pop);
  } else {
    pop.style.display='none';
  }
}
function makeDraggable(pop){
  if(pop._dragBound) return;
  pop._dragBound=true;
  const head=pop.querySelector('.gear-title')||pop;
  head.style.cursor='grab';
  let sx,sy,startL,startT,drag=false;
  head.addEventListener('mousedown',e=>{
    if(e.target.closest('.gear-close')) return;
    drag=true;
    const r=pop.getBoundingClientRect();   // popover es position:fixed -> coords de viewport
    startL=r.left; startT=r.top; sx=e.clientX; sy=e.clientY;
    pop.style.right='auto'; pop.style.bottom='auto';
    pop.style.left=startL+'px'; pop.style.top=startT+'px';
    head.style.cursor='grabbing'; e.preventDefault();
  });
  window.addEventListener('mousemove',e=>{
    if(!drag) return;
    pop.style.left=(startL+e.clientX-sx)+'px';
    pop.style.top=(startT+e.clientY-sy)+'px';
  });
  window.addEventListener('mouseup',()=>{ if(drag){drag=false; head.style.cursor='grab';} });
}

function bindEditor(){
  ['ed-nombre','ed-adddate','ed-w','ed-h','ed-iw','ed-bopa','ed-fcolor','ed-border','ed-bw','ed-imgmode'].forEach(id=>{
    const el=document.getElementById(id); if(el){ el.addEventListener('input',onEditChange); el.addEventListener('change',onEditChange); }
  });
  document.querySelectorAll('.ed-lineinput').forEach(el=>el.addEventListener('input',updatePreview));
  const drop=document.getElementById('ed-imgdrop'), file=document.getElementById('ed-imgfile');
  if(drop&&file){
    drop.onclick=()=>file.click();
    const take=f=>{ _editDesign._imgFile=f; _editDesign._imgURL=URL.createObjectURL(f); drop.textContent='Imagen: '+f.name; updatePreview(); };
    file.onchange=()=>{ if(file.files[0]) take(file.files[0]); };
    ['dragover','dragleave','drop'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.toggle('over',ev==='dragover');}));
    drop.addEventListener('drop',e=>{ const f=e.dataTransfer.files[0]; if(f) take(f); });
  }
}

function onEditChange(e){
  const t=e.target, g=id=>document.getElementById(id);
  if(t.id==='ed-w') g('ed-wv').textContent=t.value;
  if(t.id==='ed-h') g('ed-hv').textContent=t.value;
  if(t.id==='ed-iw') g('ed-iwv').textContent=t.value+'%';
  if(t.id==='ed-bopa') g('ed-bopav').textContent=t.value+'%';
  if(t.id==='ed-border'){ const r=g('ed-bwrow'); if(r) r.style.display=t.checked?'inline-flex':'none'; }
  updatePreview();
}

function collectParams(){
  const g=id=>document.getElementById(id);
  const lines=[...document.querySelectorAll('.ed-lineinput')].map(i=>i.value).filter(v=>v.trim()!=='');
  const prev=_editDesign.params||{};
  return {
    firmante:prev.firmante||{value:(window.__userName||''),ask:false},
    reason:prev.reason||{value:'',ask:false},
    location:prev.location||{value:'',ask:false},
    contact:prev.contact||{value:'',ask:false},
    mode:prev.mode||'approval',
    visible:true,
    page:prev.page||1,
    stamp_source:'custom',
    stamp_lines:lines,
    add_date:g('ed-adddate')?g('ed-adddate').checked:true,
    stamp_w:g('ed-w')?+g('ed-w').value:400,
    stamp_h:g('ed-h')?+g('ed-h').value:120,
    box:prev.box||{x1:40,y1:40,x2:440,y2:160},
    image_mode:g('ed-imgmode')?g('ed-imgmode').value:'left',
    image_width:(g('ed-iw')?g('ed-iw').value:40)+'%',
    image_opacity:1.0,
    text_opacity:1.0,
    fill_opacity:(g('ed-bopa')?+g('ed-bopa').value:0)/100,
    fill_color:(g('ed-fcolor')?g('ed-fcolor').value:'#FFFFFF'),
    border:g('ed-border')?g('ed-border').checked:false,
    border_width:g('ed-bw')?+g('ed-bw').value:(prev.border_width||2)
  };
}

function stampLines(p){
  // Devuelve las lineas reales. Si no hay lineas NI fecha -> vacio (para que la imagen ocupe todo).
  const L=(p.stamp_lines&&p.stamp_lines.length)?p.stamp_lines.slice():[];
  if(p.add_date!==false) L.push(new Date().toLocaleDateString('es'));
  return L;
}

function updatePreview(){
  const p=collectParams();
  const stamp=document.getElementById('ed-stamp');
  if(!stamp) return;
  stamp.style.display='block';
  stamp.style.border = p.border ? `${p.border_width}px solid #185FA5` : '1px dashed #c9d4e3';
  const W=p.stamp_w||400, H=p.stamp_h||120;
  const scale=Math.min(230/W, 150/H, 0.6);
  stamp.style.width=Math.round(W*scale)+'px';
  stamp.style.height=Math.round(H*scale)+'px';
  stamp.style.boxSizing='border-box';
  stamp.style.overflow='hidden';
  stamp.style.padding='0';
  // El FONDO del sello lleva la opacidad (como hara el API): recuadro blanco semitransparente.
  // fill_opacity = opacidad del fondo. 0 = transparente (se ve el documento), 1 = blanco solido.
  stamp.style.background='transparent';

  const lines=stampLines(p);
  const hasText=lines.length>0;
  const txtHTML=`<div class="sps-txt" style="color:#000">${lines.map(l=>`<div>${esc(l)}</div>`).join('')}</div>`;
  const hasImg=!!_editDesign._imgURL;
  const imgURL=_editDesign._imgURL;
  const bgLayer=`<div class="sps-bgfill" style="position:absolute;inset:0;background:${p.fill_color||'#FFFFFF'};opacity:${p.fill_opacity??0}"></div>`;

  let content='';
  if(hasImg && !hasText){
    // SIN texto: la imagen ocupa el 100% del sello, COVER (proporcion preservada aunque se corte)
    content=`<div style="position:absolute;inset:0;background:url('${imgURL}') center/cover no-repeat"></div>`;
  } else if(hasImg && p.image_mode==='background'){
    content=`
      <div class="sps-bgimg" style="position:absolute;inset:0;background:url('${imgURL}') center/contain no-repeat"></div>
      <div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;padding:6px">${txtHTML}</div>`;
  } else if(hasImg && p.image_mode==='left'){
    const iwPct=parseInt(p.image_width)||40;
    content=`
      <div style="position:absolute;inset:0;display:flex;align-items:center;gap:6px;padding:6px">
        <div class="sps-img" style="width:${iwPct}%;height:100%;flex-shrink:0;background:url('${imgURL}') left center/contain no-repeat"></div>
        <div style="flex:1;min-width:0">${txtHTML}</div>
      </div>`;
  } else if(hasText){
    content=`<div style="position:absolute;inset:0;display:flex;align-items:center;padding:6px">${txtHTML}</div>`;
  } else {
    content=`<div style="position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:#b9c4d4;font-size:9px">Agrega texto o un logo</div>`;
  }
  stamp.innerHTML=bgLayer+content;
  // FIX1: el engranaje se posiciona junto al sello (esquina superior derecha del sello)
  const gear=document.getElementById('ed-gear');
  if(gear){
    const sw=parseInt(stamp.style.width)||140, sh=parseInt(stamp.style.height)||72;
    const left=24, bottomCss=28; // coincide con #ed-stamp en CSS
    gear.style.left=(left+sw-12)+'px';
    gear.style.right='auto';
    gear.style.top='auto';
    gear.style.bottom=(bottomCss+sh-12)+'px';
  }
}

async function saveDesign(){
  const btn=event&&event.target?event.target.closest('button'):null;
  if(btn){ btn.disabled=true; btn.style.opacity='0.6'; }
  try{
    const params=collectParams();
    const nombre=document.getElementById('ed-nombre').value||'Mi firma';
    const es_default=document.getElementById('ed-default').checked?1:0;
    // 1) si hay imagen NUEVA seleccionada, subirla primero
    let image_path=_editDesign.image_path||null;
    if(_editDesign._imgFile){
      const fd=new FormData(); fd.append('image', _editDesign._imgFile);
      const r=await fetch(API+'?action=design_image',{method:'POST',credentials:'same-origin',body:fd});
      const j=await r.json();
      if(j&&j.ok){ image_path=j.path; } else { alert('No se pudo subir la imagen'); if(btn){btn.disabled=false;btn.style.opacity='';} return; }
    }
    // 2) guardar el diseno con la ruta de imagen
    await apiPost('design_save',{id:_editDesign.id, nombre, params, es_default, image_path});
    closeEditor();
    renderDisenos();
  }catch(e){ alert('Error al guardar: '+e.message); if(btn){btn.disabled=false;btn.style.opacity='';} }
}

function closeEditor(){ const pop=document.getElementById("ed-gearpop"); if(pop && pop.parentElement===document.body) pop.remove(); drawer.classList.remove("wide"); closeDrawer(); _editDesign=null; }
