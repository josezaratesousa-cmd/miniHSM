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
  const bw = p.border ? (p.border_width||1) : 0;
  const border = bw ? `${bw}px solid var(--accent)` : '1px solid transparent';
  const imgSrc = imageURL || (imagePath ? imagePath : '');
  const img = (imgSrc && p.image_mode!=='none')
    ? `<div class="sps-img" style="opacity:${p.image_opacity??1}"><img src="${imgSrc}"></div>` : '';
  const txt = `<div class="sps-txt" style="opacity:${p.text_opacity??1}">${lines.map(l=>`<div>${esc(l)}</div>`).join('')}</div>`;
  const inner = (p.image_mode==='left') ? img+txt : txt;
  return `<div class="dprev-stamp" style="border:${border}">${inner}</div>`;
}
function designCardHTML(x){
  const p = x.params||{};
  const prev = p.visible
    ? `<div class="dprev">${stampHTML(p, x.image_path)}</div>`
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
  drawer.classList.add('wide');
  openDrawer(editorHTML(d));
  bindEditor();
  updatePreview();
}

function defaultParams(){
  return {
    firmante:{value:(window.__userName||'Usuario'),ask:false},
    reason:{value:'Aprobación del documento',ask:false},
    location:{value:'',ask:false},
    contact:{value:'',ask:false},
    mode:'approval', visible:true, page:1,
    box:{x1:40,y1:40,x2:440,y2:160},
    stamp_source:'attributes', stamp_lines:[],
    image_mode:'left', image_width:'40%',
    image_opacity:0.5, text_opacity:1.0,
    border:true, border_width:2
  };
}

function fldData(key,label,p){
  const v=p[key]||{value:'',ask:false};
  return `<div class="ed-data-row">
    <input type="text" data-k="${key}" data-f="value" value="${esc(v.value)}" placeholder="${label}">
    <label class="ed-ask" title="Pedir este dato al cargar el documento"><input type="checkbox" data-k="${key}" data-f="ask" ${v.ask?'checked':''}> pedir</label>
  </div>`;
}

function editorHTML(d){
  const p=d.params;
  const custom = p.stamp_source==='custom';
  return `
  <div class="drawer-head"><span class="dh-title">${svg("pencil")} ${d.id?'Editar':'Nuevo'} diseño</span><button class="dh-close" onclick="closeEditor()">&times;</button></div>
  <div class="editor">
    <div class="ed-form">
      <div class="fld"><label>Nombre del diseño</label><input type="text" id="ed-nombre" value="${esc(d.nombre)}"></div>

      <p class="ed-group">DATOS DE LA FIRMA <span class="ed-hint">(✓ pedir = se solicita al firmar)</span></p>
      ${fldData('firmante','Firmante',p)}
      ${fldData('reason','Razón',p)}
      ${fldData('location','Lugar',p)}
      ${fldData('contact','Contacto',p)}
      <div class="fld" style="margin-top:8px"><label>Modo de firma</label>
        <select id="ed-mode"><option value="approval"${p.mode==='approval'?' selected':''}>Aprobación — permite más firmas</option><option value="certify"${p.mode==='certify'?' selected':''}>Certificación — sella</option></select>
      </div>

      <p class="ed-group" style="display:flex;align-items:center;justify-content:space-between">SELLO VISIBLE
        <label class="sw"><input type="checkbox" id="ed-visible" ${p.visible?'checked':''}> <span></span></label>
      </p>
      <div id="ed-stampbox" style="${p.visible?'':'display:none'}">
        <div class="fld" id="ed-linesbox">
          <label>Texto del sello (una línea por renglón)</label>
          <textarea id="ed-lines" rows="4" placeholder="Si lo dejas vacío, se muestra el nombre del firmante.">${esc((p.stamp_lines||[]).join('\n'))}</textarea>
          <div class="ed-hint2">Escribe lo que quieres que se vea. Comodines: %(signer)s = firmante, %(ts)s = fecha.</div>
        </div>
        <div class="fld"><label>Imagen del sello (logo / firma)</label>
          <div class="dropzone" id="ed-imgdrop">${d.image_path?'Imagen cargada · clic para cambiar':'<span style=\"font-size:20px\">${svg("upload",20)}</span> Arrastra una imagen'}</div>
          <input type="file" id="ed-imgfile" accept="image/*" hidden>
        </div>
        <div class="grid2-e">
          <div class="fld"><label>Página</label><input type="number" id="ed-page" value="${p.page||1}" min="1"></div>
          <div class="fld"><label>Disposición</label><select id="ed-imgmode"><option value="left"${p.image_mode==='left'?' selected':''}>Imagen izq · texto der</option><option value="background"${p.image_mode==='background'?' selected':''}>Fondo</option></select></div>
        </div>
        <div class="fld"><label>Posición en página (x1,y1,x2,y2)</label>
          <div class="coords4"><input type="number" id="ed-x1" value="${p.box.x1}"><input type="number" id="ed-y1" value="${p.box.y1}"><input type="number" id="ed-x2" value="${p.box.x2}"><input type="number" id="ed-y2" value="${p.box.y2}"></div>
        </div>
        <div class="rng"><label>Opacidad imagen</label><input type="range" id="ed-iopa" min="0" max="100" value="${Math.round((p.image_opacity??.5)*100)}"><span id="ed-iopav">${Math.round((p.image_opacity??.5)*100)}%</span></div>
        <div class="rng"><label>Opacidad texto</label><input type="range" id="ed-topa" min="20" max="100" value="${Math.round((p.text_opacity??1)*100)}"><span id="ed-topav">${Math.round((p.text_opacity??1)*100)}%</span></div>
        <div class="rng"><label>Borde</label><input type="checkbox" id="ed-border" ${p.border?'checked':''}><input type="number" id="ed-bw" value="${p.border_width||2}" min="0" max="10" style="width:54px"><span class="muted2">px</span></div>
      </div>

      <label class="ed-defcheck"><input type="checkbox" id="ed-default" ${d.es_default?'checked':''}> Usar como predeterminado</label>
    </div>

    <div class="ed-preview">
      <div class="ed-prev-label">${svg("eye")} Vista previa</div>
      <div class="ed-sheet" id="ed-sheet">
        <div class="sheet-lines"><span></span><span></span><span></span><span></span></div>
        <div class="sheet-stamp" id="ed-stamp"></div>
      </div>
      <div class="ed-prev-actions">
        <button class="btn" onclick="saveDesign()">${svg("save")} Guardar diseño</button>
        <button class="btn ghost" onclick="closeEditor()">Cancelar</button>
      </div>
    </div>
  </div>`;
}

function bindEditor(){
  const ids=['ed-nombre','ed-mode','ed-visible','ed-lines','ed-page','ed-imgmode','ed-x1','ed-y1','ed-x2','ed-y2','ed-iopa','ed-topa','ed-border','ed-bw'];
  ids.forEach(id=>{ const el=document.getElementById(id); if(el){ el.addEventListener('input',onEditChange); el.addEventListener('change',onEditChange); }});
  document.querySelectorAll('.ed-data-row input').forEach(el=>el.addEventListener('input',onEditChange));
  const drop=document.getElementById('ed-imgdrop'), file=document.getElementById('ed-imgfile');
  if(drop&&file){
    drop.onclick=()=>file.click();
    file.onchange=()=>{ if(file.files[0]){ _editDesign._imgFile=file.files[0]; _editDesign._imgURL=URL.createObjectURL(file.files[0]); drop.textContent='Imagen: '+file.files[0].name; updatePreview(); }};
    ['dragover','dragleave','drop'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.toggle('over',ev==='dragover');}));
    drop.addEventListener('drop',e=>{ const f=e.dataTransfer.files[0]; if(f){ _editDesign._imgFile=f; _editDesign._imgURL=URL.createObjectURL(f); drop.textContent='Imagen: '+f.name; updatePreview(); }});
  }
}

function onEditChange(e){
  const t=e.target;
  if(t.id==='ed-visible'){ document.getElementById('ed-stampbox').style.display=t.checked?'':'none'; }
  if(t.id==='ed-iopa'){ document.getElementById('ed-iopav').textContent=t.value+'%'; }
  if(t.id==='ed-topa'){ document.getElementById('ed-topav').textContent=t.value+'%'; }
  updatePreview();
}

function collectParams(){
  const g=id=>document.getElementById(id);
  const dataRows={};
  document.querySelectorAll('.ed-data-row').forEach(row=>{
    const v=row.querySelector('[data-f=value]'), a=row.querySelector('[data-f=ask]');
    dataRows[v.dataset.k]={value:v.value, ask:a.checked};
  });
  return {
    ...dataRows,
    mode:g('ed-mode').value,
    visible:g('ed-visible').checked,
    page:parseInt(g('ed-page').value)||1,
    box:{x1:+g('ed-x1').value,y1:+g('ed-y1').value,x2:+g('ed-x2').value,y2:+g('ed-y2').value},
    stamp_source:'custom',
    stamp_lines:(g('ed-lines').value||'').split('\n').filter(l=>l.trim()!==''),
    image_mode:g('ed-imgmode').value,
    image_width:'40%',
    image_opacity:(+g('ed-iopa').value)/100,
    text_opacity:(+g('ed-topa').value)/100,
    border:g('ed-border').checked,
    border_width:+g('ed-bw').value
  };
}

function stampLines(p){
  // Texto personalizado tiene prioridad; los placeholders se ven literales en preview.
  if(p.stamp_lines && p.stamp_lines.length) return p.stamp_lines.slice();
  // Por defecto: solo el nombre del firmante (sin "Firmado por:").
  const n = (p.firmante && p.firmante.value) ? p.firmante.value : '';
  return n ? [n] : ['(nombre del firmante)'];
}
function updatePreview(){
  const p=collectParams();
  const stamp=document.getElementById('ed-stamp');
  if(!stamp) return;
  if(!p.visible){ stamp.style.display='none'; return; }
  stamp.style.display='flex';
  stamp.style.border = p.border? `${p.border_width}px solid var(--accent)`:'none';
  // texto del sello: SOLO lo que el usuario escribe. Sin etiquetas concatenadas.
  // Si no hay texto, por defecto el nombre del firmante.
  let lines = stampLines(p);
  const hasImg=!!_editDesign._imgURL;
  const img = hasImg? `<div class="sps-img" style="opacity:${p.image_opacity}"><img src="${_editDesign._imgURL}" style="max-width:100%;max-height:100%"></div>`:'';
  const txt = `<div class="sps-txt" style="opacity:${p.text_opacity}">${lines.map(l=>`<div>${esc(l)}</div>`).join('')}</div>`;
  stamp.innerHTML = (p.image_mode==='left') ? img+txt : txt;
}

async function saveDesign(){
  const params=collectParams();
  const nombre=document.getElementById('ed-nombre').value||'Sin nombre';
  const es_default=document.getElementById('ed-default').checked?1:0;
  const r=await apiPost('design_save',{id:_editDesign.id, nombre, params, es_default});
  // (la imagen se subira en una subfase posterior: image_path)
  closeEditor();
  renderDisenos();
}

function closeEditor(){ drawer.classList.remove('wide'); closeDrawer(); _editDesign=null; }
