SET NAMES utf8mb4;
ALTER TABLE sign_requests ADD COLUMN design_id BIGINT UNSIGNED NULL AFTER device_id;
