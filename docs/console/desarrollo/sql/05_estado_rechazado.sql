SET NAMES utf8mb4;
ALTER TABLE sign_requests MODIFY COLUMN estado ENUM('pendiente','entregado','firmado','rechazado','error') NOT NULL DEFAULT 'pendiente';
