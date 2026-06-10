# Xami — Contexto: Firmado con Atestación Blockchain

> Documento de fundamentos. Explica QUÉ estamos construyendo y POR QUÉ, antes de los
> detalles técnicos del CHANGELOG. Escrito para no perder el hilo conceptual.
> Fecha: 2026-06-10

---

## El problema que Xami resuelve

Firmar un documento digitalmente plantea tres preguntas que un verificador necesita
responder con certeza:

1. **¿Quién firmó?** — identidad del firmante
2. **¿La firma es auténtica?** — que solo el poseedor de la clave pudo crearla
3. **¿Cuándo se firmó?** — un momento que nadie pueda falsificar ni retroceder

Xami responde las tres de forma que NO haya que confiar en el propio dispositivo:
la confianza descansa en criptografía estándar y en blockchain, no en "creerle al Xami".

---

## Las dos piezas

**miniHSM (el dispositivo, ESP32-S3 = "Xami-A1"):**
Posee la clave privada y FIRMA. Es deliberadamente "tonto": solo sabe criptografía.
Recibe un digest, lo firma, devuelve la firma. Su clave nunca sale del dispositivo.

**serverHSM (el optimizador, Python/FastAPI):**
Orquesta. Sabe qué firmar, prepara documentos (PAdES/XAdES/CAdES), maneja internet,
y SELLA en blockchain. Le manda comandos simples al miniHSM y arma el resultado final.

Principio rector de todo el diseño: **minimizar la confianza depositada en el
dispositivo.** Cada decisión refuerza que la verdad sea verificable de forma
independiente al Xami.

---

## Las tres capas de prueba

Cada respuesta verificable del Xami (firma de documento, info de device, log de
auditoría) se construye con tres capas que responden las tres preguntas:

### Capa 1 — IDENTIDAD + AUTENTICIDAD (la firma del device)
El miniHSM firma con su clave privada (ECDSA P-256). Esto SOLO lo puede hacer él,
porque solo él tiene la clave. La firma demuestra:
- Quién: este device específico (su pubkey lo identifica)
- Autenticidad: solo el poseedor de la privada pudo firmar (proof of possession)
Evoluciona a Verifiable Credential W3C 2.0 con COSE (ver CHANGELOG Bloque 3).

### Capa 2 — AUTORIZACIÓN (quién pidió la operación)
Antes de firmar, el miniHSM valida que quien pide tiene permiso:
- HMAC (autenticación del solicitante: el serverHSM legítimo)
- VaultStamping/KUser (autorización demostrable: prueba de que fue autorizado)
Ver CHANGELOG Bloque 5.

### Capa 3 — TIEMPO (el sello blockchain)
El serverHSM sella la atestación en stamping.io → se ancla en blockchain vía
Merkle tree + IPFS. Esto prueba CUÁNDO, sin depender del reloj del Xami ni de su
palabra. Ver CHANGELOG Bloque 7.

---

## Reparto device vs server (importante)

| Tarea | Quién | Por qué |
|-------|-------|---------|
| Firmar (la clave) | **miniHSM** | solo él tiene la clave privada |
| Generar atestación de identidad | **miniHSM** | la firma con su clave (proof of possession) |
| Sellar en blockchain | **serverHSM** | tiene internet y la API key de stamping.io |
| Orquestar (PAdES/XAdES/CAdES) | **serverHSM** | conoce el contexto del documento |

CONFIRMADO: la información del DEVICE también queda atestada y sellada. El reparto es:
el **device FIRMA** su atestación (identidad + posesión de clave), el **server SELLA**
esa atestación en blockchain (prueba de tiempo). Cada uno aporta lo que solo él puede.

---

## El principio clave del sello: el Xami atesta y olvida

El Xami NO almacena el sello (ni el trxid). Solo lo RESPONDE en el momento. La custodia
de la prueba es del VERIFICADOR; la memoria de la prueba vive en blockchain.

Esto garantiza que el Xami no puede falsificar ni borrar su propia prueba de tiempo:
la prueba existe independientemente de él, anclada en blockchain vía stamping.io.

---

## Estado de esta prueba (2026-06-10)

- miniHSM Xami-A1 operativo: arranca, configura WiFi, firma disponible en /sign
- serverHSM operativo en https://fileserver.locker/serverHSM/api/
- stamping.io PROBADO: bearer token funciona, trxid=sha1(evidence) confirmado, code 200
- PRÓXIMO PASO DE PRUEBA: validar el flujo de firmado real end-to-end
  (server genera token → miniHSM firma digest → se sella → respuesta con las 3 capas)
