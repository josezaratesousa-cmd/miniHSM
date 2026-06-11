<?php
/**
 * Autenticación por sesión para console.
 * Login contra users (bcrypt). Deriva tenant_id/user_id/rol de la sesión.
 */
require_once __DIR__ . '/db.php';

function auth_start(): void {
    if (session_status() === PHP_SESSION_NONE) {
        session_set_cookie_params(['httponly' => true, 'samesite' => 'Lax']);
        session_start();
    }
}

function auth_login(string $email, string $password): bool {
    auth_start();
    $st = xami_db()->prepare(
        'SELECT id, tenant_id, password_hash, es_gestor, must_rotate_password, estado
         FROM users WHERE email = ? LIMIT 1'
    );
    $st->execute([$email]);
    $u = $st->fetch();
    if (!$u || $u['estado'] !== 'activo') return false;
    if (!password_verify($password, $u['password_hash'])) return false;

    session_regenerate_id(true);
    $_SESSION['uid']        = (int)$u['id'];
    $_SESSION['tenant_id']  = (int)$u['tenant_id'];
    $_SESSION['es_gestor']  = (bool)$u['es_gestor'];
    $_SESSION['must_rotate']= (bool)$u['must_rotate_password'];
    $_SESSION['email']      = $email;
    return true;
}

function auth_check(): bool {
    auth_start();
    return isset($_SESSION['uid']);
}

function auth_user(): ?array {
    auth_start();
    if (!isset($_SESSION['uid'])) return null;
    return [
        'uid'         => $_SESSION['uid'],
        'tenant_id'   => $_SESSION['tenant_id'],
        'es_gestor'   => $_SESSION['es_gestor'] ?? false,
        'must_rotate' => $_SESSION['must_rotate'] ?? false,
        'email'       => $_SESSION['email'] ?? '',
    ];
}

function auth_logout(): void {
    auth_start();
    $_SESSION = [];
    session_destroy();
}

function auth_require(): array {
    if (!auth_check()) {
        header('Location: /console/');
        exit;
    }
    return auth_user();
}
