SET NAMES utf8mb4;
INSERT INTO sign_requests (tenant_id,user_id,device_id,request_id,filename,size_bytes,pages,origen,estado,facturable,created_at,signed_at,closed_at,motivo_rechazo) VALUES
(1,1,'fe4dfede3b10c54b','req-p001','contrato-arriendo.pdf',253952,6,'Mi firma personal','pendiente',1,'2026-06-11 15:42:00',NULL,NULL,NULL),
(1,1,'fe4dfede3b10c54b','req-p002','acta-reunion.pdf',97280,2,'Mi firma personal','pendiente',1,'2026-06-11 14:05:00',NULL,NULL,NULL),
(1,1,'fe4dfede3b10c54b','req-p003','factura-0012.pdf',62464,1,'Firma app facturacion','entregado',1,'2026-06-11 09:18:00',NULL,NULL,NULL),
(1,1,'fe4dfede3b10c54b','req-f001','poder-notarial.pdf',421888,8,'Mi firma personal','firmado',1,'2026-06-10 16:58:00','2026-06-10 17:30:00',NULL,NULL),
(1,1,'fe4dfede3b10c54b','req-r001','addenda-v2.pdf',122880,3,'Mi firma personal','rechazado',0,'2026-06-09 10:40:00',NULL,'2026-06-09 11:02:00','Datos del arrendatario incorrectos');
INSERT INTO sign_events (sign_request_id,tipo,actor,created_at)
SELECT id,'enviado','Usuario Demo','2026-06-10 16:58:00' FROM sign_requests WHERE request_id='req-f001';
INSERT INTO sign_events (sign_request_id,tipo,actor,created_at)
SELECT id,'abierto','Usuario Demo','2026-06-10 17:12:00' FROM sign_requests WHERE request_id='req-f001';
INSERT INTO sign_events (sign_request_id,tipo,actor,created_at)
SELECT id,'procesado','fe4dfede3b10c54b','2026-06-10 17:29:00' FROM sign_requests WHERE request_id='req-f001';
INSERT INTO sign_events (sign_request_id,tipo,actor,created_at)
SELECT id,'firmado','fe4dfede3b10c54b','2026-06-10 17:30:00' FROM sign_requests WHERE request_id='req-f001';
