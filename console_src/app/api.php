<?php
/* Xami · Panel — API JSON (guardián, aislado por user/tenant de la sesión) */
require_once __DIR__ . '/../lib/auth.php';
header('Content-Type: application/json; charset=utf-8');

$u = auth_user();
if (!$u) { http_response_code(401); echo json_encode(['error'=>'no auth']); exit; }
$uid = $u['uid']; $tid = $u['tenant_id'];
$action = $_GET['action'] ?? '';
$db = xami_db();

try {
  if ($action === 'counts') {
    $st = $db->prepare("SELECT estado, COUNT(*) n FROM sign_requests WHERE user_id=? GROUP BY estado");
    $st->execute([$uid]);
    $m = ['pendiente'=>0,'entregado'=>0,'firmado'=>0,'rechazado'=>0,'error'=>0];
    foreach ($st as $r) $m[$r['estado']] = (int)$r['n'];
    echo json_encode([
      'pendientes' => $m['pendiente'] + $m['entregado'],
      'firmados'   => $m['firmado'],
      'rechazados' => $m['rechazado'],
    ]);
    exit;
  }

  if ($action === 'list') {
    $bandeja = $_GET['bandeja'] ?? 'pendientes';
    $map = [
      'pendientes' => ["estado IN ('pendiente','entregado')", 'created_at DESC'],
      'firmados'   => ["estado='firmado'", 'signed_at DESC'],
      'rechazados' => ["estado='rechazado'", 'closed_at DESC'],
    ];
    [$where,$order] = $map[$bandeja] ?? $map['pendientes'];
    $st = $db->prepare("SELECT id,filename,size_bytes,pages,origen,estado,created_at,signed_at,closed_at,motivo_rechazo
                        FROM sign_requests WHERE user_id=? AND $where ORDER BY $order");
    $st->execute([$uid]);
    echo json_encode(['items'=>$st->fetchAll()]);
    exit;
  }

  if ($action === 'detail') {
    $id = (int)($_GET['id'] ?? 0);
    $st = $db->prepare("SELECT * FROM sign_requests WHERE id=? AND user_id=? LIMIT 1");
    $st->execute([$id,$uid]);
    $item = $st->fetch();
    if (!$item) { http_response_code(404); echo json_encode(['error'=>'not found']); exit; }
    $ev = $db->prepare("SELECT tipo,actor,created_at FROM sign_events WHERE sign_request_id=? ORDER BY created_at");
    $ev->execute([$id]);
    echo json_encode(['item'=>$item,'events'=>$ev->fetchAll()]);
    exit;
  }

  if ($action === 'devices') {
    $st = $db->prepare("SELECT device_id, estado, firmware, last_seen FROM devices WHERE user_id=? ORDER BY created_at");
    $st->execute([$uid]);
    $items = [];
    foreach ($st as $d) {
      $online = $d['last_seen'] && (time() - strtotime($d['last_seen'])) < 600;
      $items[] = ['device_id'=>$d['device_id'],'alias'=>null,'estado'=>$d['estado'],'firmware'=>$d['firmware'],'online'=>$online];
    }
    echo json_encode(['items'=>$items]);
    exit;
  }


  if ($action === 'designs') {
    $st = $db->prepare("SELECT id,nombre,params,image_path,es_default,updated_at FROM sign_designs WHERE user_id=? ORDER BY es_default DESC, nombre");
    $st->execute([$uid]);
    $items = [];
    foreach ($st as $d) { $d['params'] = json_decode($d['params'], true); $items[] = $d; }
    echo json_encode(['items'=>$items]);
    exit;
  }

  if ($action === 'design_get') {
    $id = (int)($_GET['id'] ?? 0);
    $st = $db->prepare("SELECT id,nombre,params,image_path,es_default FROM sign_designs WHERE id=? AND user_id=? LIMIT 1");
    $st->execute([$id,$uid]);
    $d = $st->fetch();
    if (!$d) { http_response_code(404); echo json_encode(['error'=>'not found']); exit; }
    $d['params'] = json_decode($d['params'], true);
    echo json_encode($d);
    exit;
  }

  if ($action === 'design_save' && $_SERVER['REQUEST_METHOD']==='POST') {
    $in = json_decode(file_get_contents('php://input'), true) ?: [];
    $id     = (int)($in['id'] ?? 0);
    $nombre = trim($in['nombre'] ?? 'Sin nombre');
    $params = json_encode($in['params'] ?? new stdClass());
    $def    = !empty($in['es_default']) ? 1 : 0;
    $img    = isset($in['image_path']) ? ($in['image_path'] ?: null) : null;
    if ($def) { $db->prepare("UPDATE sign_designs SET es_default=0 WHERE user_id=?")->execute([$uid]); }
    if ($id) {
      $st = $db->prepare("UPDATE sign_designs SET nombre=?,params=?,es_default=?,image_path=? WHERE id=? AND user_id=?");
      $st->execute([$nombre,$params,$def,$img,$id,$uid]);
    } else {
      $st = $db->prepare("INSERT INTO sign_designs (user_id,nombre,params,es_default,image_path) VALUES (?,?,?,?,?)");
      $st->execute([$uid,$nombre,$params,$def,$img]);
      $id = (int)$db->lastInsertId();
    }
    echo json_encode(['ok'=>true,'id'=>$id]);
    exit;
  }

  if ($action === 'design_delete' && $_SERVER['REQUEST_METHOD']==='POST') {
    $in = json_decode(file_get_contents('php://input'), true) ?: [];
    $id = (int)($in['id'] ?? 0);
    $st = $db->prepare("DELETE FROM sign_designs WHERE id=? AND user_id=?");
    $st->execute([$id,$uid]);
    echo json_encode(['ok'=>true]);
    exit;
  }


  if ($action === 'design_image' && $_SERVER['REQUEST_METHOD']==='POST') {
    if (empty($_FILES['image']) || $_FILES['image']['error'] !== UPLOAD_ERR_OK) {
      http_response_code(400); echo json_encode(['error'=>'no file']); exit;
    }
    $f = $_FILES['image'];
    if ($f['size'] > 2*1024*1024) { http_response_code(400); echo json_encode(['error'=>'too big']); exit; }
    $info = @getimagesize($f['tmp_name']);
    if (!$info) { http_response_code(400); echo json_encode(['error'=>'not image']); exit; }
    $ext = ['image/png'=>'png','image/jpeg'=>'jpg','image/gif'=>'gif','image/webp'=>'webp'][$info['mime']] ?? null;
    if (!$ext) { http_response_code(400); echo json_encode(['error'=>'bad type']); exit; }
    // Almacenamiento PRIVADO por tenant, FUERA del docroot. Nada accesible por URL directa.
    $rel = 'uploads/signatures';
    $dir = '/home/xami/tenants/'.$tid.'/'.$rel;
    if (!is_dir($dir)) @mkdir($dir, 0700, true);
    $fname = 'u'.$uid.'_'.bin2hex(random_bytes(8)).'.'.$ext;
    $dest = $dir.'/'.$fname;
    if (!move_uploaded_file($f['tmp_name'], $dest)) { http_response_code(500); echo json_encode(['error'=>'save failed']); exit; }
    @chmod($dest, 0600);
    // image_path = ruta RELATIVA dentro de tenants/{tid}/. El servicio file.php la resuelve.
    echo json_encode(['ok'=>true, 'path'=>$rel.'/'.$fname]);
    exit;
  }

  http_response_code(400);
  echo json_encode(['error'=>'acción desconocida']);
} catch (Throwable $e) {
  http_response_code(500);
  echo json_encode(['error'=>$e->getMessage()]);
}
