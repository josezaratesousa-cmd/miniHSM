# Endpoints de Console

> Contrato de endpoints. Separa lo que CONSOLE expone (PHP, propio) de lo que
> CONSOLE consume del OPTIMIZADOR (api.xami.run) actuando de guardián.
> Estado: DISEÑO. Se irá ajustando al construir cada panel.

## Convenciones
- console expone endpoints PHP bajo `/console/api/...` (JSON).
- Autenticación por sesión PHP; cada request resuelve tenant_id+user_id+rol desde
  la sesión, NUNCA desde parámetros del cliente.
- console valida permisos en xami_db ANTES de llamar al optimizador.
- Roles: U=usuario dueño, G=gestor, S=super-admin.

---

## 1. Endpoints que CONSOLE expone (propios, PHP)

### Autenticación (todos los roles)
| Método | Ruta | Entrada | Salida | Rol |
|---|---|---|---|---|
| POST | /console/api/login | email, password | sesión + rol(es) | público |
| POST | /console/api/logout | — | ok | U/G/S |
| POST | /console/api/rotate-password | actual, nueva | ok | U/G/S |

### Panel de USUARIO
| Método | Ruta | Entrada | Salida | Rol |
|---|---|---|---|---|
| GET | /console/api/my/devices | — | lista de devices del usuario + estado | U |
| GET | /console/api/my/devices/{id}/prefs | — | sign_prefs del device | U |
| PUT | /console/api/my/devices/{id}/prefs | sign_prefs(json) | ok | U |
| POST | /console/api/my/devices/{id}/sign | pdf + overrides | request_id | U |
| GET | /console/api/my/devices/{id}/queue | — | jobs pendientes/estado | U |
| POST | /console/api/my/devices/{id}/poke | — | ok (solo misma red) | U |
| GET | /console/api/my/signs/{request_id} | — | estado + link descarga | U |

### Panel de GESTOR
| Método | Ruta | Entrada | Salida | Rol |
|---|---|---|---|---|
| GET | /console/api/mgr/dashboard | — | inventario, saldo, métricas | G |
| GET | /console/api/mgr/devices | — | devices del tenant + estado polling | G |
| POST | /console/api/mgr/users | datos usuario + identidad CA | user creado | G |
| PUT | /console/api/mgr/devices/{id}/assign | user_id | ok | G |
| PUT | /console/api/mgr/devices/{id}/disable | — | ok (inhabilitar) | G |
| POST | /console/api/mgr/devices/{id}/rotate-secret | cuál secreto | ok | G |
| GET | /console/api/mgr/activity | rango fechas | series para gráficos | G |
| GET | /console/api/mgr/logs | filtros | logs recientes soporte | G |

### Panel de SUPER-ADMIN (xami.run)
| Método | Ruta | Entrada | Salida | Rol |
|---|---|---|---|---|
| POST | /console/api/adm/tenants | nombre, código | tenant + link OTP | S |
| POST | /console/api/adm/tenants/{id}/onboarding-link | — | nuevo link OTP 48h | S |
| GET | /console/api/adm/orphans | — | devices huérfanos | S |
| PUT | /console/api/adm/devices/{id}/assign-tenant | tenant_id | ok | S |
| PUT | /console/api/adm/tenants/{id}/saldo | delta | ok | S |
| PUT | /console/api/adm/tenants/{id}/ca-config | ca_config(json) | ok | S |

---

## 2. Endpoints del OPTIMIZADOR que console consume (api.xami.run)

> NO se modifican (regla: no tocar optimizador). console los llama como guardián,
> tras validar permisos. Leídos del código real (optimizer/api/).

### Devices (prefix /devices)
| Método | Ruta | Uso desde console |
|---|---|---|
| GET | /devices/ | inventario/estado de devices (gestor) |
| GET | /devices/{device_id} | estado de un device (polling, last seen) |
| POST | /devices/heartbeat | (lo usa el device, no console) |
| POST | /devices/match | (lo usa el device, no console) |
| POST | /devices/{device_id}/jobs | ENCOLAR firma (panel usuario -> guardián) |
| GET | /devices/{device_id}/jobs/{request_id} | estado del job/cola |
| POST | /devices/{device_id}/jobs/{request_id}/result | (lo usa el device) |

### Firmas / signatures (prefix /v1/signatures)
| Método | Ruta | Uso desde console |
|---|---|---|
| POST | /v1/signatures/pdf | firmar PDF (flujo de firma) |
| GET | /v1/signatures/pdf/{pid} | estado de la firma |
| GET | /v1/signatures/pdf/{pid}/download | descargar PDF firmado |
| GET | /v1/signatures/ca.pem | cadena CA |

### Otros (main.py)
| Método | Ruta | Uso desde console |
|---|---|---|
| GET | /health | salud del optimizador (monitoreo) |
| GET | /device, /cert, /csr | info del device/cert (lectura) |

---

## 3. Notas
- DT-guardián: hoy el optimizador no valida usuarios; console debe ser el único
  que lo llame. Asegurar esa frontera (red interna / token de servicio). Ver DT9.
- DT-inhabilitado: el encolamiento debe respetar device inhabilitado. Hoy no lo
  hace; queda como deuda (DT3) a coordinar con el optimizador.
- "poke"/apurar cola: solo tiene efecto si el navegador del usuario está en la
  misma red que el device (el server no alcanza la LAN). Ver DT8.
