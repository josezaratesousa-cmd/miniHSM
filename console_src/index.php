<?php
/* Xami · Panel — Login */
require_once __DIR__ . '/lib/auth.php';
auth_start();
if (auth_check()) { header('Location: /console/app/'); exit; }
$err = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $email = trim($_POST['user'] ?? '');
    $pass  = $_POST['pass'] ?? '';
    if (auth_login($email, $pass)) {
        header('Location: /console/app/'); exit;
    }
    $err = 'Usuario o contraseña incorrectos.';
}
?>
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Xami · Panel — Acceso</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
<style>
:root{--bg:#0a0e1a;--panel:#111a2e;--ink:#eaf0fb;--muted:#8da2c0;--line:#1e2c4a;--accent:#4db8ff;--accent2:#7c5cff;--grad:linear-gradient(135deg,#4db8ff,#7c5cff)}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Inter',sans-serif;background:var(--bg);color:var(--ink);min-height:100vh;display:flex;align-items:center;justify-content:center;
background-image:radial-gradient(900px 500px at 70% -10%,rgba(124,92,255,.18),transparent),radial-gradient(700px 400px at 10% 20%,rgba(77,184,255,.12),transparent);padding:24px}
.card{width:100%;max-width:400px;background:linear-gradient(160deg,#16213d,var(--panel));border:1px solid var(--line);border-radius:20px;padding:42px 36px;box-shadow:0 20px 60px rgba(0,0,0,.5)}
.brand{font-weight:800;font-size:26px;letter-spacing:1px;text-align:center;margin-bottom:6px}
.brand span{background:var(--grad);-webkit-background-clip:text;background-clip:text;color:transparent}
.sub{text-align:center;color:var(--muted);font-size:14px;margin-bottom:30px}
label{display:block;font-size:13px;color:var(--muted);margin:16px 0 7px;font-weight:500}
input{width:100%;padding:13px 15px;background:#0a1120;border:1px solid var(--line);border-radius:11px;color:var(--ink);font-size:15px;font-family:inherit;transition:.2s}
input:focus{outline:none;border-color:var(--accent);box-shadow:0 0 0 3px rgba(77,184,255,.12)}
.btn{width:100%;margin-top:26px;padding:14px;border:none;border-radius:11px;background:var(--grad);color:#fff;font-weight:600;font-size:15px;cursor:pointer;transition:.25s;font-family:inherit}
.btn:hover{transform:translateY(-2px);box-shadow:0 12px 30px rgba(124,92,255,.35)}
.err{background:rgba(255,180,84,.1);border:1px solid rgba(255,180,84,.3);color:#ffb454;padding:11px 14px;border-radius:10px;font-size:13px;margin-bottom:18px;text-align:center}
.back{display:block;text-align:center;margin-top:22px;color:var(--muted);font-size:13px;text-decoration:none}
.back:hover{color:var(--ink)}
.lock{text-align:center;font-size:30px;margin-bottom:14px}
</style>
</head>
<body>
<form class="card" method="post">
  <div class="lock">&#128274;</div>
  <div class="brand">Xami<span>.</span>Panel</div>
  <div class="sub">Acceso a la consola de administración</div>
  <?php if($err): ?><div class="err"><?=htmlspecialchars($err)?></div><?php endif; ?>
  <label for="user">Usuario o correo</label>
  <input type="text" id="user" name="user" placeholder="tu@correo.com" autocomplete="username">
  <label for="pass">Contraseña</label>
  <input type="password" id="pass" name="pass" placeholder="••••••••" autocomplete="current-password">
  <button class="btn" type="submit">Ingresar &rarr;</button>
  <a class="back" href="/">&larr; Volver al inicio</a>
</form>
</body>
</html>
