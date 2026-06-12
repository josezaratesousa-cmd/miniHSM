-- sign_designs: plantillas reutilizables de firma (apariencia del sello + datos)
SET NAMES utf8mb4;
CREATE TABLE IF NOT EXISTS sign_designs (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id     BIGINT UNSIGNED NOT NULL,
  nombre      VARCHAR(120) NOT NULL,
  params      JSON NOT NULL,
  image_path  VARCHAR(255) NULL,
  es_default  TINYINT(1) NOT NULL DEFAULT 0,
  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_sd_user (user_id),
  CONSTRAINT fk_sd_user FOREIGN KEY (user_id) REFERENCES users(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
