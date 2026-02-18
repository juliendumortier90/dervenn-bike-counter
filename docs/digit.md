# Fonctionnement de `digit.ino`

## Rôle
Le programme `digit.ino` est le récepteur LoRa. Il reçoit les compteurs depuis l’émetteur (`counter.ino`), les affiche sur la liaison série, et sauvegarde périodiquement les valeurs en EEPROM. Il peut aussi déclencher une sauvegarde distante via un bouton local.

## Matériel et broches
- Module LoRa E220: `RX=2`, `TX=3`, `M0=4`, `M1=5`, `AUX=6`
- Bouton local (appui long): `PIN_REMOTE_SAVE_BUTTON=8` en `INPUT_PULLUP`
- EEPROM: `EEPROM_ADDR_TOTAL=0`, `EEPROM_ADDR_SESSION=sizeof(int)`

## Démarrage
- Initialise `Serial` à 9600 bauds.
- Configure le bouton en entrée pull‑up.
- Active le watchdog matériel (timeout ~2s) pour redémarrer en cas de blocage.
- Initialise le module LoRa en `MODE_0_NORMAL`.
- Recharge `lastTotal` et `lastSession` depuis l’EEPROM.
- Affiche les valeurs restaurées sur la console série.

## Boucle principale
Trois tâches sont exécutées à chaque itération:
- Rafraîchissement du watchdog.
- Réception LoRa.
- Sauvegarde périodique en EEPROM.
- Gestion du bouton.

## Réception LoRa
- Le récepteur lit un message texte.
- Si le message est `SAVED_OK`, il affiche une confirmation puis s’arrête.
- Si le message contient une virgule, il est interprété comme `total,session`.
- Les valeurs reçues mettent à jour `lastTotal` et `lastSession` et passent `newData` à `true`.

## Sauvegarde périodique
- Toutes les 3 heures (`SAVE_INTERVAL`), si de nouvelles données ont été reçues (`newData=true`), les valeurs sont enregistrées en EEPROM.
- Après la sauvegarde, `newData` repasse à `false`.

## Bouton (appui long)
- Appui long 4 secondes: envoie `SAVE` à l’émetteur et sauvegarde immédiatement en EEPROM.
- Ce mécanisme permet de déclencher à distance la persistance des compteurs côté émetteur.

## Protocole LoRa
- Message de données: `"total,session"` depuis l’émetteur.
- Commande de sauvegarde: `"SAVE"` vers l’émetteur.
- Accusé de réception: `"SAVED_OK"` depuis l’émetteur.

## EEPROM
- `lastTotal` et `lastSession` sont stockés en `int`.
- Une validation simple remet à zéro si les valeurs sortent de `0..999999`.

## Traces série utiles
- `=== Récepteur LoRa E220 prêt ===`
- `Données reçues → Total: X | Session: Y`
- `Appui long → Envoi commande SAVE...`
- `Sauvegarde locale → Total: X | Session: Y`
