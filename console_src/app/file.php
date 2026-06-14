<?php
/* Servicio de archivos privados del tenant (guardián).
   Ningún archivo del tenant es accesible por URL directa; SOLO por aquí,
   validando sesión + pertenencia al usuario/tenant. */
require_once __DIR__ . '/../lib/auth.php';

$u = auth_user();
if (!$u) { http_response_code(401); exit('no auth'); }
$uid = $u['uid']; $tid = $u['tenant_id'];

$type = $_GET['type'] ?? '';
$db = xami_db();

// Raíz privada de almacenamiento (FUERA del docroot)
$BASE = '/home/xami/tenants';

if ($type === 'signature') {
  $id = (int)($_GET['id'] ?? 0);
  // el diseño debe pertenecer al usuario de la sesión
  $st = $db->prepare("SELECT image_path FROM sign_designs WHERE id=? AND user_id=? LIMIT 1");
  $st->execute([$id, $uid]);
  $row = $st->fetch();
  if (!$row || !$row['image_path']) { http_response_code(404); exit('not found'); }

  // image_path guarda la ruta RELATIVA dentro de tenants/{tid}/...
  $rel = ltrim($row['image_path'], '/');
  $full = $BASE . '/' . $tid . '/' . $rel;
  // anti path traversal: el path real resuelto debe seguir dentro de la carpeta del tenant
  $realBase = realpath($BASE . '/' . $tid);
  $realFull = realpath($full);
  if ($realFull === false || strpos($realFull, $realBase) !== 0) { http_response_code(403); exit('forbidden'); }
  if (!is_file($realFull)) { http_response_code(404); exit('gone'); }

  $info = @getimagesize($realFull);
  $mime = $info['mime'] ?? 'application/octet-stream';
  header('Content-Type: ' . $mime);
  header('Cache-Control: private, max-age=300');
  header('Content-Length: ' . filesize($realFull));
  readfile($realFull);
  exit;
}

if ($type === 'pdf_preview') {
  // sirve un PDF recien subido (zona privada del tenant) para previsualizar con pdf.js.
  // valida que la ruta este confinada al tenant de la sesion.
  $rel = ltrim($_GET['path'] ?? '', '/');
  // solo permitimos rutas dentro de uploads/documents
  if (strpos($rel, 'uploads/documents/') !== 0 || strpos($rel, '..') !== false) {
    http_response_code(403); exit('forbidden');
  }
  $full = $BASE . '/' . $tid . '/' . $rel;
  $realBase = realpath($BASE . '/' . $tid);
  $realFull = realpath($full);
  if ($realFull === false || strpos($realFull, $realBase) !== 0) { http_response_code(403); exit('forbidden'); }
  if (!is_file($realFull)) { http_response_code(404); exit('gone'); }
  header('Content-Type: application/pdf');
  header('Cache-Control: private, max-age=120');
  header('Content-Length: ' . filesize($realFull));
  readfile($realFull);
  exit;
}

http_response_code(400); exit('bad request');
