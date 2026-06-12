'use strict';
const API = '/app/api.php';
const ICONS={
 check:'<path d="M5 12l5 5L20 7"/>',
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
function openNuevo(){
  openDrawer(`
    <div class="drawer-head"><span class="dh-title">${svg("pencil")} Nueva firma</span><button class="dh-close" onclick="closeDrawer()">&times;</button></div>
    <div class="fld"><label>Dispositivo / identidad</label><select id="nf-dev"><option>Mi firma personal (fe4dfede…)</option></select></div>
    <div class="fld"><label>Documento</label><div class="dropzone" id="nf-drop"><span style="font-size:26px">${svg("upload",20)}</span><br>Arrastra el PDF aquí o haz clic</div></div>
    <div class="fld"><label>Motivo (opcional)</label><input type="text" placeholder="Aprobación"></div>
    <div class="tl-date" style="margin-bottom:14px">${svg("info")} Usará tus preferencias guardadas</div>
    <div style="display:flex;gap:8px"><button class="btn" onclick="alert('Encolado: pendiente de implementar en la siguiente subfase')">${svg("send")} Enviar a firmar</button><button class="btn ghost" onclick="closeDrawer()">Cancelar</button></div>`);
}

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
  const bg = `<div style="position:absolute;inset:0;background:#fff;opacity:${p.fill_opacity??0}"></div>`;
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
    image_opacity:1.0, text_opacity:1.0, fill_opacity:0.0,
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
  ['ed-nombre','ed-adddate','ed-w','ed-h','ed-iw','ed-bopa','ed-border','ed-bw','ed-imgmode'].forEach(id=>{
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
  const bgLayer=`<div class="sps-bgfill" style="position:absolute;inset:0;background:#fff;opacity:${p.fill_opacity??0}"></div>`;

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
