<?php
/* Xami · Panel de usuario (inbox) — shell */
require_once __DIR__ . '/../lib/auth.php';
$u = auth_require();
?>
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Xami · Panel</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
<link rel="stylesheet" href="/assets/css/app.css">
<script>(function(){try{if((localStorage.getItem("xami_theme")||"light")==="dark")document.documentElement.setAttribute("data-theme","dark");}catch(e){}})();</script>
</head>
<body>
<div id="app">

  <header class="topbar">
    <div class="tb-left">
      <button id="toggleSidebar" class="icon-btn" title="Mostrar/ocultar menú" aria-label="Menú">&#9776;</button>
      <a class="brand" href="/app/"><span class="brand-mark">&#128274;</span> Xami</a>
    </div>
    <div class="tb-right">
      <button id="themeToggle" class="icon-btn" title="Cambiar tema" aria-label="Cambiar tema">&#9790;</button>
      <button class="icon-btn" title="Notificaciones" aria-label="Notificaciones">&#128276;</button>
      <div class="profile">
        <?php $em = $u['email'] ?? 'usuario@xami.run'; $ini = strtoupper(substr($em,0,2)); ?><span class="avatar"><?= htmlspecialchars($ini) ?></span>
        <span class="profile-email"><?= htmlspecialchars(explode('@', $em)[0]) ?></span>
        <a class="logout" href="/logout.php" title="Salir">&#9211;</a>
      </div>
    </div>
  </header>

  <div class="body">
    <aside id="sidebar" class="sidebar">
      <nav>
        <p class="nav-group">INBOX</p>
        <a class="nav-item active" data-view="pendientes" href="#pendientes">
          <span class="ni-ico">&#128338;</span><span class="ni-txt">Pendientes</span><span class="ni-count" id="c-pendientes">0</span>
        </a>
        <a class="nav-item" data-view="firmados" href="#firmados">
          <span class="ni-ico">&#10003;</span><span class="ni-txt">Firmados</span><span class="ni-count" id="c-firmados">0</span>
        </a>
        <a class="nav-item" data-view="rechazados" href="#rechazados">
          <span class="ni-ico">&#10007;</span><span class="ni-txt">Rechazados</span><span class="ni-count" id="c-rechazados">0</span>
        </a>
        <p class="nav-group">CONFIGURACIÓN</p>
        <a class="nav-item" data-view="dispositivos" href="#dispositivos">
          <span class="ni-ico">&#128241;</span><span class="ni-txt">Mis dispositivos</span>
        </a>
        <a class="nav-item" data-view="preferencias" href="#preferencias">
          <span class="ni-ico">&#9881;</span><span class="ni-txt">Mis preferencias</span>
        </a>
        <a class="nav-item" data-view="consumo" href="#consumo">
          <span class="ni-ico">&#128202;</span><span class="ni-txt">Mi consumo</span>
        </a>
      </nav>
      <div class="resizer" id="resizer" title="Arrastra para redimensionar"></div>
    </aside>

    <main class="main" id="main">
      <div class="main-loading">Cargando…</div>
    </main>
  </div>

  <div id="drawer" class="drawer"><div class="drawer-inner" id="drawerInner"></div></div>
  <div id="overlay" class="overlay"></div>
</div>
<script src="/assets/js/app.js"></script>
</body>
</html>
