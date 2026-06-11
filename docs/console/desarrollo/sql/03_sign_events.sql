-- sign_events: trazabilidad (linea de tiempo) por solicitud de firma
SET NAMES utf8mb4;
CREATE TABLE IF NOT EXISTS sign_events (
  id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  sign_request_id BIGINT UNSIGNED NOT NULL,
  tipo            ENUM('enviado','abierto','procesado','firmado','rechazado') NOT NULL,
  actor           VARCHAR(180) NULL,
  meta            JSON NULL,
  created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_se_request (sign_request_id),
  CONSTRAINT fk_se_request FOREIGN KEY (sign_request_id) REFERENCES sign_requests(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
