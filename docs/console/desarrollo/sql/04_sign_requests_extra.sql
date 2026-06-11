-- Columnas extra para sign_requests segun el diseno del inbox
SET NAMES utf8mb4;
ALTER TABLE sign_requests
  ADD COLUMN filename     VARCHAR(255) NULL AFTER request_id,
  ADD COLUMN size_bytes   INT NULL AFTER filename,
  ADD COLUMN pages        INT NULL AFTER size_bytes,
  ADD COLUMN origen        VARCHAR(120) NULL AFTER pages,
  ADD COLUMN motivo_rechazo VARCHAR(255) NULL AFTER estado,
  ADD COLUMN signed_at     DATETIME NULL AFTER created_at,
  ADD COLUMN closed_at     DATETIME NULL AFTER signed_at;
