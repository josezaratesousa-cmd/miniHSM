-- ============================================================
-- xami_db — Esquema de console (multi-tenant)
-- Ver docs/BD/DISENO_BD.md. Motor InnoDB, charset utf8mb4.
-- Idempotente: CREATE TABLE IF NOT EXISTS.
-- ============================================================
SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS=0;

-- 1) tenants ---------------------------------------------------
CREATE TABLE IF NOT EXISTS tenants (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  codigo_login  VARCHAR(32)  NOT NULL,
  nombre        VARCHAR(150) NOT NULL,
  estado        ENUM('activo','suspendido','baja') NOT NULL DEFAULT 'activo',
  saldo_firmas  INT NOT NULL DEFAULT 0,
  ca_config     JSON NULL,
  created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_tenants_codigo (codigo_login)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 2) users -----------------------------------------------------
CREATE TABLE IF NOT EXISTS users (
  id                   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  tenant_id            BIGINT UNSIGNED NOT NULL,
  email                VARCHAR(180) NOT NULL,
  password_hash        VARCHAR(255) NOT NULL,
  es_gestor            TINYINT(1) NOT NULL DEFAULT 0,
  must_rotate_password TINYINT(1) NOT NULL DEFAULT 1,
  estado               ENUM('activo','suspendido','baja') NOT NULL DEFAULT 'activo',
  created_at           DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_users_email (email),
  KEY idx_users_tenant (tenant_id),
  CONSTRAINT fk_users_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 3) devices ---------------------------------------------------
CREATE TABLE IF NOT EXISTS devices (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id   VARCHAR(32) NOT NULL,
  tenant_id   BIGINT UNSIGNED NULL,
  user_id     BIGINT UNSIGNED NULL,
  estado      ENUM('huerfano','asignado_tenant','asignado_usuario','activo','inhabilitado') NOT NULL DEFAULT 'huerfano',
  firmware    VARCHAR(40) NULL,
  last_seen   DATETIME NULL,
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_devices_devid (device_id),
  KEY idx_devices_tenant (tenant_id),
  KEY idx_devices_user (user_id),
  KEY idx_devices_estado (estado),
  CONSTRAINT fk_devices_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id),
  CONSTRAINT fk_devices_user   FOREIGN KEY (user_id)   REFERENCES users(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 4) device_prefs (1:1 devices) --------------------------------
CREATE TABLE IF NOT EXISTS device_prefs (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id   BIGINT UNSIGNED NOT NULL,
  sign_prefs  JSON NULL,
  updated_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_prefs_device (device_id),
  CONSTRAINT fk_prefs_device FOREIGN KEY (device_id) REFERENCES devices(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 5) ca_identity (1:1 devices) ---------------------------------
CREATE TABLE IF NOT EXISTS ca_identity (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  device_id   BIGINT UNSIGNED NOT NULL,
  cert_data   JSON NULL,
  doc_tipo    VARCHAR(20) NULL,
  doc_numero  VARCHAR(30) NULL,
  cert_estado ENUM('pendiente','emitido','revocado') NOT NULL DEFAULT 'pendiente',
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_ca_device (device_id),
  CONSTRAINT fk_ca_device FOREIGN KEY (device_id) REFERENCES devices(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 6) onboarding_links ------------------------------------------
CREATE TABLE IF NOT EXISTS onboarding_links (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  tenant_id   BIGINT UNSIGNED NOT NULL,
  token       VARCHAR(64) NOT NULL,
  otp         VARCHAR(10) NOT NULL,
  expires_at  DATETIME NOT NULL,
  used        TINYINT(1) NOT NULL DEFAULT 0,
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_onb_token (token),
  KEY idx_onb_expires (expires_at),
  CONSTRAINT fk_onb_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 7) sign_requests ---------------------------------------------
CREATE TABLE IF NOT EXISTS sign_requests (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  tenant_id   BIGINT UNSIGNED NOT NULL,
  user_id     BIGINT UNSIGNED NOT NULL,
  device_id   VARCHAR(32) NOT NULL,
  request_id  VARCHAR(48) NOT NULL,
  estado      ENUM('pendiente','entregado','firmado','error') NOT NULL DEFAULT 'pendiente',
  facturable  TINYINT(1) NOT NULL DEFAULT 1,
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_sr_tenant_date (tenant_id, created_at),
  KEY idx_sr_device (device_id),
  KEY idx_sr_request (request_id),
  CONSTRAINT fk_sr_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id),
  CONSTRAINT fk_sr_user   FOREIGN KEY (user_id)   REFERENCES users(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 8) billing_events --------------------------------------------
CREATE TABLE IF NOT EXISTS billing_events (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  tenant_id   BIGINT UNSIGNED NOT NULL,
  device_id   VARCHAR(32) NULL,
  request_id  VARCHAR(48) NULL,
  tipo        ENUM('consumo','recarga','ajuste') NOT NULL,
  delta_saldo INT NOT NULL,
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_be_tenant_date (tenant_id, created_at),
  CONSTRAINT fk_be_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

SET FOREIGN_KEY_CHECKS=1;
-- FIN del esquema
