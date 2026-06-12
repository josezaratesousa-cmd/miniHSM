-- Diseño estándar por defecto para el usuario demo (toma datos del sistema)
SET NAMES utf8mb4;
INSERT INTO sign_designs (id, user_id, nombre, params, es_default) VALUES
(1, 1, 'Estándar',
 JSON_OBJECT(
   'firmante', JSON_OBJECT('value','Usuario Demo','ask',false),
   'reason',   JSON_OBJECT('value','Aprobación del documento','ask',false),
   'location', JSON_OBJECT('value','Lima, Perú','ask',false),
   'contact',  JSON_OBJECT('value','','ask',false),
   'mode','approval',
   'visible', true,
   'page', 1,
   'box', JSON_OBJECT('x1',40,'y1',40,'x2',440,'y2',160),
   'stamp_source','attributes',
   'stamp_lines', JSON_ARRAY(),
   'image_mode','left',
   'image_width','40%',
   'image_opacity',0.5,
   'text_opacity',1.0,
   'border',true,
   'border_width',2
 ),
 1)
ON DUPLICATE KEY UPDATE params=VALUES(params);
