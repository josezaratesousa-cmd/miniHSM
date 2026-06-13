# DISEÑO — Servicio Wallet EVM (tokenizacion / firmas atestadas)   [2026-06-13]

## Proposito
HD wallet en el chip para FIRMAR (no cripto-trading): tokenizacion y firmas atestadas
on-chain en redes EVM. El chip POSEE las claves y firma; xami.run prepara el JSON/tx y
hace el send/broadcast.

## Reparto criptografico — chip "firmante secp256k1 ciego"
- Chip: secp256k1 + BIP-39/BIP-32 + firma ECDSA de hash 32B (low-s EIP-2 + recovery id v).
  Expone la pubkey de cada cuenta. NO lleva Keccak, RLP ni logica EVM.
- xami.run: Keccak-256 (address = ultimos 20B de Keccak(pubkey); y hash de la tx), RLP,
  chainId/EIP-155/1559, broadcast.
- Firmware nuevo: habilitar secp256k1 (mbedTLS ECP, coexiste con P-256); wordlist BIP-39
  (2048 palabras ~13KB); BIP-39 (PBKDF2-HMAC-SHA512) + BIP-32 (HMAC-SHA512). Sin Keccak.

## Persistencia (opcion a)
- La SEED maestra se guarda CIFRADA con la KEK del chip (igual que custody).
- La MNEMONICA (palabras) NUNCA se persiste; solo vive en RAM durante la ceremonia.
- La priv de cada cuenta nunca sale del chip; el chip solo firma.

## Crear wallet (ceremonia en LAN)
- POST /wallet/create { words: 12|15|18|21|24, clientPub: <pubkey efimera EC del cliente> }
  -> chip genera entropia -> mnemonica; deriva seed; mantiene mnemonica SOLO en RAM.
  -> devuelve la mnemonica CIFRADA por ECIES hacia clientPub (nunca en claro por LAN).
- El cliente descifra, muestra las palabras para apuntar y NO las retiene para el paso siguiente.
- Verificacion por RECONOCIMIENTO (apto movil, sin teclear):
  - el chip muestra la lista con 3 palabras SUSTITUIDAS por otras del diccionario (plausibles);
    recuerda los indices alterados. (entrega tambien cifrada a clientPub)
  - POST /wallet/verify { wrongIndices:[i,j,k] } -> si == alterados: COMMIT (persiste seed
    cifrada, borra mnemonica de RAM, deriva cuenta 0); si no: descarta y se regenera.
  - Nota: la web NO debe tener las palabras correctas a la vista durante la verificacion.
- Commit -> { ok, account:{ index:0, path, address, pubkey, origin:"derived" } }

## Derivar address nueva
- POST /wallet/address/new -> deriva m/44'/60'/0'/0/i (i++)
  -> { index, path, address, pubkey, origin:"derived" }

## Importar (cuentas NO derivadas del chip)
- Por mnemonica: reconstruye la wallet HD (re-deriva las MISMAS addresses) -> recuperacion / cambio de chip.
- Por clave privada: una cuenta suelta (no HD, no genera derivadas).
- Entrada CIFRADA por ECIES hacia la pubkey del CHIP (igual que la ceremonia de custodia).
- Se guardan cifradas y marcadas origin:"imported". Permite dar de alta cuando el chip no tiene wallet.

## Marcado de origen (clave para la atestacion)
- origin "derived"  -> la clave nacio en el chip y nunca salio: prueba de exclusividad fuerte.
- origin "imported" -> vino de fuera: sin garantia de exclusividad.
- El /device (VC) expone el origin por cuenta -> el verificador on-chain sabe que garantia tiene.

## Identidad on-chain y migracion de chip (ERC-1056 / did:ethr)
- La address es el ancla de identidad on-chain (did:ethr:<address>, resoluble via ERC-1056).
- El chip (did:key P-256) es el custodio reemplazable; atesta que controla la address.
- Migrar de chip sin perder identidad, dos vias:
  (a) Reimportar mnemonica/clave -> misma address; red de seguridad; queda "imported".
  (b) changeOwner en ERC-1056 -> la identidad (address ancla) no cambia; el control pasa a una
      clave NUEVA derivada en el chip nuevo (nunca sale) -> sigue "derived". Camino fuerte.
      El chip solo FIRMA la tx (la arma xami.run). Igual setAttribute/addDelegate para publicar
      la did:key del chip como delegado/atributo del DID.

## Firmar
- Flujo de jobs (como custody): xami.run encola { walletAccount:i, hash:<32B Keccak> },
  el chip firma con secp256k1 (low-s) y devuelve (r, s, v). xami.run arma la tx/RLP/send.

## GET /wallet
- { hasWallet, accounts:[ { index, path, address, pubkey, origin } ] }
