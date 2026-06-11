<?php
/**
 * Conexión PDO a xami_db para console.
 * Lee el .env de forma robusta (el password puede contener '~', '=', etc.),
 * por eso NO usa parse_ini_file. Devuelve una instancia PDO singleton.
 */
function xami_env(string $key, ?string $default = null): ?string {
    static $env = null;
    if ($env === null) {
        $env = [];
        $path = __DIR__ . '/../.env';
        if (is_readable($path)) {
            foreach (file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
                if ($line === '' || $line[0] === '#') continue;
                $pos = strpos($line, '=');
                if ($pos === false) continue;
                $k = trim(substr($line, 0, $pos));
                $v = trim(substr($line, $pos + 1));
                $env[$k] = $v;
            }
        }
    }
    return $env[$key] ?? $default;
}

function xami_db(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        $name = xami_env('DB_NAME');
        $user = xami_env('DB_USER');
        $pass = xami_env('DB_PASSWORD');
        $host = xami_env('DB_HOST', 'localhost');
        $dsn  = "mysql:host=$host;dbname=$name;charset=utf8mb4";
        $pdo  = new PDO($dsn, $user, $pass, [
            PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES   => false,
        ]);
    }
    return $pdo;
}
