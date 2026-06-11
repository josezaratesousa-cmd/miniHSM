-- ============================================================
-- xami_db — Datos de prueba (seed) para desarrollo
-- Idempotente con INSERT ... ON DUPLICATE KEY UPDATE.
-- Password de prueba: "xami1234" (hash bcrypt). CAMBIAR en prod.
-- ============================================================
SET NAMES utf8mb4;

-- Tenant de prueba
INSERT INTO tenants (id, codigo_login, nombre, estado, saldo_firmas, ca_config)
VALUES (1, 'demo', 'Tenant Demo (pruebas)', 'activo', 1000,
        JSON_OBJECT('ca','uanataca','perfil','PJ','tsa_url','http://tsa.uanataca.com/tsa'))
ON DUPLICATE KEY UPDATE nombre=VALUES(nombre), saldo_firmas=VALUES(saldo_firmas);

-- Usuario dueño de prueba (no gestor). Hash bcrypt de "xami1234".
INSERT INTO users (id, tenant_id, email, password_hash, es_gestor, must_rotate_password, estado)
VALUES (1, 1, 'demo@xami.run',
        '$2y$10$bmPSFjHRcVYo2Zs7wVqfFOGFm2p8HT6Zn3m3R8zf.k0EBRUEiFKua',
        0, 0, 'activo')
ON DUPLICATE KEY UPDATE tenant_id=VALUES(tenant_id);

-- Device real ya emparejado, asignado al usuario de prueba -> ACTIVO
INSERT INTO devices (id, device_id, tenant_id, user_id, estado, firmware, last_seen)
VALUES (1, 'fe4dfede3b10c54b', 1, 1, 'activo', 'firmware-v35', NOW())
ON DUPLICATE KEY UPDATE tenant_id=VALUES(tenant_id), user_id=VALUES(user_id), estado=VALUES(estado);

-- Preferencias de firma por defecto para ese device
INSERT INTO device_prefs (id, device_id, sign_prefs)
VALUES (1, 1, JSON_OBJECT('name','Usuario Demo','reason','Aprobacion','location','Lima',
        'visible',true,'page',1,'box','50,50,250,130','image_mode','left',
        'border',true,'mode','approval'))
ON DUPLICATE KEY UPDATE sign_prefs=VALUES(sign_prefs);
