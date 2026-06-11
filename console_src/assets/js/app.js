'use strict';
const API = '/app/api.php';

/* ---------- Tema claro/oscuro (recordado en localStorage) ---------- */
(function initTheme(){
  var t = localStorage.getItem('xami_theme') || 'light';
  if (t === 'dark') document.documentElement.setAttribute('data-theme','dark');
})();
function applyThemeIcon(){
  var dark = document.documentElement.getAttribute('data-theme') === 'dark';
  var btn = document.getElementById('themeToggle');
  if (btn) btn.innerHTML = dark ? '\u2600' : '\u263e';
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
function fmtSize(b){ if(b==null)return '—'; if(b<1024)return b+' B'; if(b<1048576)return Math.round(b/1024)+' KB'; return (b/1048576).toFixed(1)+' MB'; }
function fmtDate(s){ if(!s)return '—'; const d=new Date(s.replace(' ','T')); return d.toLocaleDateString('es',{day:'2-digit',month:'short'})+', '+d.toLocaleTimeString('es',{hour:'2-digit',minute:'2-digit'}); }
function esc(s){ const d=document.createElement('div'); d.textContent=s==null?'':s; return d.innerHTML; }

/* ---------- Vistas ---------- */
function renderView(view){
  if (['pendientes','firmados','rechazados'].includes(view)) return renderBandeja(view);
  if (view === 'dispositivos') return renderDispositivos();
  if (view === 'preferencias') return renderSimple('Mis preferencias','Próximamente: configura tu identidad y apariencia de firma.');
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
  const nuevoBtn = view==='pendientes' ? `<button class="btn sm" id="btnNuevo">&#9998; Nuevo</button>` : '';
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
      : `<button class="btn sm" onclick="event.stopPropagation();signDoc(${it.id})">Firmar</button><button class="btn sm danger" onclick="event.stopPropagation();rejectDoc(${it.id})">&#10007;</button>`;
  } else if (view==='firmados'){
    action = `<button class="btn sm ghost" onclick="event.stopPropagation();dl(${it.id})">&#8595;</button>`;
  } else {
    action = `<span class="badge" title="${esc(it.motivo_rechazo||'')}">motivo</span>`;
  }
  const icon = view==='firmados'?'&#9989;':(view==='rechazados'?'&#10060;':'&#128196;');
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
      <div class="preview-box"><span style="font-size:38px">&#128196;</span><span>Vista previa del PDF</span><span style="font-size:11px">${it.pages||'—'} página(s)</span></div>
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
  if (it.estado==='pendiente') return `<button class="btn" onclick="signDoc(${it.id})">&#9998; Firmar</button><button class="btn danger" onclick="rejectDoc(${it.id})">&#10007; Rechazar</button>`;
  if (it.estado==='firmado')   return `<button class="btn ghost" onclick="dl(${it.id})">&#8595; Descargar firmado</button>`;
  if (it.estado==='rechazado') return `<div class="tl-date">Motivo: ${esc(it.motivo_rechazo||'—')}</div>`;
  return `<span class="badge cola">En cola</span>`;
}

function timelineHTML(ev){
  const meta = {enviado:['&#10148;','Enviado a firmar',''],abierto:['&#128065;','Abierto / revisado',''],procesado:['&#9881;','Procesado por el dispositivo',''],firmado:['&#10003;','Firmado','ok'],rechazado:['&#10007;','Rechazado','bad']};
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
    <div class="drawer-head"><span class="dh-title">&#9998; Nueva firma</span><button class="dh-close" onclick="closeDrawer()">&times;</button></div>
    <div class="fld"><label>Dispositivo / identidad</label><select id="nf-dev"><option>Mi firma personal (fe4dfede…)</option></select></div>
    <div class="fld"><label>Documento</label><div class="dropzone" id="nf-drop"><span style="font-size:26px">&#128228;</span><br>Arrastra el PDF aquí o haz clic</div></div>
    <div class="fld"><label>Motivo (opcional)</label><input type="text" placeholder="Aprobación"></div>
    <div class="tl-date" style="margin-bottom:14px">&#9432; Usará tus preferencias guardadas</div>
    <div style="display:flex;gap:8px"><button class="btn" onclick="alert('Encolado: pendiente de implementar en la siguiente subfase')">&#10148; Enviar a firmar</button><button class="btn ghost" onclick="closeDrawer()">Cancelar</button></div>`);
}

/* ---------- Dispositivos ---------- */
async function renderDispositivos(){
  main.innerHTML = `<div class="main-loading">Cargando…</div>`;
  let d; try{ d = await apiGet('devices'); }catch(e){ main.innerHTML=`<div class="empty">Error: ${esc(e.message)}</div>`; return; }
  const rows = d.items.map(x=>`<div class="list-row"><span class="ico-doc">&#128241;</span>
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
