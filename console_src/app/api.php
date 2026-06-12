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


  if ($action === 'pdf_upload' && $_SERVER['REQUEST_METHOD']==='POST') {
    if (empty($_FILES['pdf']) || $_FILES['pdf']['error'] !== UPLOAD_ERR_OK) {
      http_response_code(400); echo json_encode(['error'=>'no file']); exit;
    }
    $f = $_FILES['pdf'];
    if ($f['size'] > 20*1024*1024) { http_response_code(400); echo json_encode(['error'=>'too big']); exit; }
    // validar que sea PDF real
    $fi = finfo_open(FILEINFO_MIME_TYPE);
    $mime = finfo_file($fi, $f['tmp_name']); finfo_close($fi);
    if ($mime !== 'application/pdf') { http_response_code(400); echo json_encode(['error'=>'not pdf']); exit; }

    // guardar en zona privada del tenant
    $rel = 'uploads/documents';
    $dir = '/home/xami/tenants/'.$tid.'/'.$rel;
    if (!is_dir($dir)) @mkdir($dir, 0700, true);
    $fname = 'u'.$uid.'_'.bin2hex(random_bytes(8)).'.pdf';
    $dest = $dir.'/'.$fname;
    if (!move_uploaded_file($f['tmp_name'], $dest)) { http_response_code(500); echo json_encode(['error'=>'save failed']); exit; }
    @chmod($dest, 0600);

    // medir el PDF con pdfinfo: paginas + dimensiones de la primera pagina
    $pages = 0; $w = 0; $h = 0;
    $out = [];
    @exec('pdfinfo '.escapeshellarg($dest).' 2>/dev/null', $out);
    foreach ($out as $line) {
      if (preg_match('/^Pages:\s+(\d+)/', $line, $m)) $pages = (int)$m[1];
      if (preg_match('/^Page size:\s+([\d.]+)\s+x\s+([\d.]+)/', $line, $m)) { $w = (float)$m[1]; $h = (float)$m[2]; }
    }
    echo json_encode([
      'ok'=>true,
      'path'=>$rel.'/'.$fname,
      'filename'=>$f['name'],
      'size'=>$f['size'],
      'pages'=>$pages,
      'width'=>$w,
      'height'=>$h
    ]);
    exit;
  }


  if ($action === 'sign_send' && $_SERVER['REQUEST_METHOD']==='POST') {
    $in = json_decode(file_get_contents('php://input'), true) ?: [];
    $pdf_rel  = $in['pdf_path'] ?? '';        // uploads/documents/xxx.pdf (relativo al tenant)
    $design_id= (int)($in['design_id'] ?? 0);
    $device_id= trim($in['device_id'] ?? '');
    $page_ref = $in['page_ref'] ?? 'last';     // last | last-1 | first | n
    $page_num = (int)($in['page_num'] ?? 1);
    $box      = $in['box'] ?? null;            // "x1,y1,x2,y2" ya resuelto por el front
    $filename = $in['filename'] ?? 'documento.pdf';

    // validar el PDF dentro del tenant
    $pdf_full = '/home/xami/tenants/'.$tid.'/'.ltrim($pdf_rel,'/');
    $realBase = realpath('/home/xami/tenants/'.$tid);
    $realPdf  = realpath($pdf_full);
    if (!$realPdf || strpos($realPdf, $realBase)!==0 || !is_file($realPdf)) {
      http_response_code(400); echo json_encode(['error'=>'pdf not found']); exit;
    }

    // cargar el diseño (debe ser del usuario)
    $st = $db->prepare("SELECT * FROM sign_designs WHERE id=? AND user_id=? LIMIT 1");
    $st->execute([$design_id, $uid]);
    $design = $st->fetch(PDO::FETCH_ASSOC);
    if (!$design) { http_response_code(400); echo json_encode(['error'=>'design not found']); exit; }
    $params = json_decode($design['params'] ?: '{}', true) ?: [];

    // medir paginas del PDF para resolver page_ref
    $pages = 0; $out=[];
    @exec('pdfinfo '.escapeshellarg($realPdf).' 2>/dev/null', $out);
    foreach ($out as $line) if (preg_match('/^Pages:\s+(\d+)/',$line,$m)) $pages=(int)$m[1];
    if ($pages<1) $pages=1;
    // resolver pagina real (1-indexed)
    $page = 1;
    if ($page_ref==='last') $page=$pages;
    elseif ($page_ref==='last-1') $page=max(1,$pages-1);
    elseif ($page_ref==='first') $page=1;
    elseif ($page_ref==='n') $page=min(max(1,$page_num),$pages);

    // armar texto del sello desde stamp_lines + fecha
    $lines = isset($params['stamp_lines'])&&is_array($params['stamp_lines']) ? $params['stamp_lines'] : [];
    if (!empty($params['add_date'])) $lines[] = date('d/m/Y');
    $stamp_text = implode("\n", $lines);

    // construir multipart para el API
    $api = 'http://127.0.0.1:8182/v1/signatures/pdf';
    $post = [
      'file'         => new CURLFile($realPdf, 'application/pdf', $filename),
      'device_id'    => $device_id,
      'visible'      => 'true',
      'page'         => (string)$page,
      'stamp_source' => 'custom',
      'stamp_text'   => $stamp_text,
      'image_mode'   => $params['image_mode'] ?? 'left',
      'image_width'  => $params['image_width'] ?? '40%',
      'image_opacity'=> (string)($params['image_opacity'] ?? 1.0),
      'text_opacity' => (string)($params['text_opacity'] ?? 1.0),
      'fill_opacity' => (string)($params['fill_opacity'] ?? 0.0),
      'fill_color'   => $params['fill_color'] ?? '#FFFFFF',
      'border'       => !empty($params['border']) ? 'true':'false',
      'border_width' => (string)($params['border_width'] ?? 2),
      'mode'         => $params['mode'] ?? 'approval',
    ];
    if ($box) $post['box'] = $box;
    // imagen del sello si el diseño tiene
    if (!empty($design['image_path'])) {
      $img_full = '/home/xami/tenants/'.$tid.'/'.ltrim($design['image_path'],'/');
      if (is_file($img_full)) $post['stamp_image'] = new CURLFile($img_full, mime_content_type($img_full), basename($img_full));
    }

    $ch = curl_init($api);
    curl_setopt_array($ch, [CURLOPT_POST=>true, CURLOPT_POSTFIELDS=>$post, CURLOPT_RETURNTRANSFER=>true, CURLOPT_TIMEOUT=>60]);
    $resp = curl_exec($ch); $code = curl_getinfo($ch, CURLINFO_HTTP_CODE); curl_close($ch);
    if ($code!==200) { http_response_code(502); echo json_encode(['error'=>'api error','code'=>$code,'detail'=>$resp]); exit; }
    $j = json_decode($resp, true);
    $request_id = $j['requestId'] ?? null;
    if (!$request_id) { http_response_code(502); echo json_encode(['error'=>'no requestId','detail'=>$resp]); exit; }

    // registrar en sign_requests + evento
    $size = filesize($realPdf);
    $st = $db->prepare("INSERT INTO sign_requests (tenant_id,user_id,device_id,design_id,request_id,filename,size_bytes,pages,origen,estado,facturable,created_at) VALUES (?,?,?,?,?,?,?,?,?, 'pendiente', 1, NOW())");
    $st->execute([$tid,$uid,$device_id,$design_id,$request_id,$filename,$size,$pages,'console']);
    $srid = (int)$db->lastInsertId();
    $db->prepare("INSERT INTO sign_events (sign_request_id,tipo,actor,meta,created_at) VALUES (?,?,?,?,NOW())")
       ->execute([$srid,'enviado',$u['email'] ?? 'console', json_encode(['page'=>$page,'box'=>$box,'design'=>$design['nombre']])]);

    echo json_encode(['ok'=>true,'request_id'=>$request_id,'sign_request_id'=>$srid,'page'=>$page]);
    exit;
  }


  if ($action === 'sign_status' && $_SERVER['REQUEST_METHOD']==='GET') {
    $request_id = trim($_GET['request_id'] ?? '');
    if (!$request_id) { http_response_code(400); echo json_encode(['error'=>'no request_id']); exit; }
    // el sign_request debe ser del usuario
    $st = $db->prepare("SELECT * FROM sign_requests WHERE request_id=? AND user_id=? LIMIT 1");
    $st->execute([$request_id,$uid]);
    $sr = $st->fetch(PDO::FETCH_ASSOC);
    if (!$sr) { http_response_code(404); echo json_encode(['error'=>'not found']); exit; }
    // si ya esta firmado/cerrado, no re-consultar
    if (in_array($sr['estado'], ['firmado','error','rechazado'])) {
      echo json_encode(['ok'=>true,'status'=>$sr['estado'],'final'=>true]); exit;
    }

    // consultar estado al API
    $ch = curl_init('http://127.0.0.1:8182/v1/signatures/pdf/'.urlencode($request_id));
    curl_setopt_array($ch, [CURLOPT_RETURNTRANSFER=>true, CURLOPT_TIMEOUT=>15]);
    $resp = curl_exec($ch); $code = curl_getinfo($ch, CURLINFO_HTTP_CODE); curl_close($ch);
    if ($code!==200) { echo json_encode(['ok'=>true,'status'=>$sr['estado'],'api_code'=>$code]); exit; }
    $j = json_decode($resp, true);
    $apistatus = strtolower($j['status'] ?? '');

    if ($apistatus === 'done') {
      // descargar el PDF firmado a la zona privada del tenant
      $ch = curl_init('http://127.0.0.1:8182/v1/signatures/pdf/'.urlencode($request_id).'/download');
      curl_setopt_array($ch, [CURLOPT_RETURNTRANSFER=>true, CURLOPT_TIMEOUT=>30]);
      $pdf = curl_exec($ch); $dc = curl_getinfo($ch, CURLINFO_HTTP_CODE); curl_close($ch);
      if ($dc===200 && substr($pdf,0,4)==='%PDF') {
        $dir = '/home/xami/tenants/'.$tid.'/signed';
        if (!is_dir($dir)) @mkdir($dir,0700,true);
        $fname = 'signed_'.$sr['id'].'_'.bin2hex(random_bytes(4)).'.pdf';
        file_put_contents($dir.'/'.$fname, $pdf); @chmod($dir.'/'.$fname,0600);
        $db->prepare("UPDATE sign_requests SET estado='firmado', signed_at=NOW() WHERE id=?")->execute([$sr['id']]);
        $db->prepare("INSERT INTO sign_events (sign_request_id,tipo,actor,meta,created_at) VALUES (?,?,?,?,NOW())")
           ->execute([$sr['id'],'firmado','device', json_encode(['signed_path'=>'signed/'.$fname])]);
        echo json_encode(['ok'=>true,'status'=>'firmado','final'=>true]); exit;
      }
      echo json_encode(['ok'=>true,'status'=>'procesando']); exit;
    }
    if ($apistatus === 'error') {
      $db->prepare("UPDATE sign_requests SET estado='error', closed_at=NOW() WHERE id=?")->execute([$sr['id']]);
      echo json_encode(['ok'=>true,'status'=>'error','final'=>true,'detail'=>$j['error']??null]); exit;
    }
    // sigue procesando
    echo json_encode(['ok'=>true,'status'=>'procesando','api'=>$apistatus]); exit;
  }

  http_response_code(400);
  echo json_encode(['error'=>'acción desconocida']);
} catch (Throwable $e) {
  http_response_code(500);
  echo json_encode(['error'=>$e->getMessage()]);
}
