# Fonctionnement de `counter.ino`

## Rôle
Le programme `counter.ino` est l’émetteur LoRa. Il détecte les passages de vélos via un capteur analogique, incrémente les compteurs, envoie les valeurs au récepteur et gère la sauvegarde en EEPROM. Il accepte aussi des commandes de sauvegarde depuis le récepteur.

## Matériel et broches
- Module LoRa E220: `RX=2`, `TX=3`, `M0=4`, `M1=5`, `AUX=6`
- Bouton local: `PIN_SAVE_BUTTON=7` en `INPUT_PULLUP`
- Capteur analogique: `SENSOR_PIN=A0`
- Seuil de détection: `DETECTION_THRESHOLD=124`
- Anti‑rebond temporel: `MIN_DELAY_BETWEEN_BIKES=800ms`
- EEPROM: `EEPROM_ADDR_TOTAL=0`, `EEPROM_ADDR_SESSION=sizeof(int)`

## Démarrage
- Initialise `Serial` à 9600 bauds.
- Configure le bouton en entrée pull‑up.
- Active le watchdog matériel (timeout ~2s) pour redémarrer en cas de blocage.
- Initialise le module LoRa en `MODE_0_NORMAL`.
- Recharge les compteurs depuis l’EEPROM.
- Affiche les valeurs initiales.

## Boucle principale
Quatre tâches sont exécutées à chaque itération:
- Rafraîchissement du watchdog.
- Détection de passage.
- Réception LoRa.
- Sauvegarde automatique.
- Gestion du bouton.

## Détection de passage
- Lit la valeur analogique sur `A0`.
- Si la valeur dépasse `DETECTION_THRESHOLD` et que `800ms` se sont écoulées depuis la dernière détection, un passage est validé.
- Incrémente `bikeCountSession` et `bikeCountTotal`.
- Envoie un message LoRa au format `total,session`.

## Réception LoRa
- Si le message reçu est `SAVE`, l’émetteur sauvegarde en EEPROM et répond `SAVED_OK`.

## Sauvegarde automatique
- Toutes les 3 heures (`SAVE_INTERVAL`), l’émetteur sauvegarde en EEPROM et envoie `SAVED_OK` au récepteur.

## Bouton local
- Appui court (< 8s): sauvegarde en EEPROM et envoie `SAVED_OK`.
- Appui long (>= 8s): remet le compteur de session à zéro et envoie `SAVED_OK`.

## Protocole LoRa
- Message de données: `"total,session"` vers le récepteur.
- Commande de sauvegarde: `"SAVE"` depuis le récepteur.
- Accusé de réception: `"SAVED_OK"` vers le récepteur.

## EEPROM
- `bikeCountTotal` et `bikeCountSession` sont stockés en `int`.
- Validation simple des valeurs lues (0..999999).

## Traces série utiles
- `Initialisation du système de comptage...`
- `Passage détecté → Total: X | Session: Y`
- `Commande SAVE reçue via LoRa !`
- `Sauvegarde → Total: X | Session: Y`
- `Compteur de session remis à zéro !`
