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

  http_response_code(400);
  echo json_encode(['error'=>'acción desconocida']);
} catch (Throwable $e) {
  http_response_code(500);
  echo json_encode(['error'=>$e->getMessage()]);
}
