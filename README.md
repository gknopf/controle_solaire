controle d'une maison autonome a l'aide d'ESP32

Les capteurs sont des circuit imprimés qui mesurent tension , courant temperatures pression etc.

Ces capteurs,organisés en secteurs de 1 a 5 circuits  selon l'utilité , communiquent  les données par esp-now vers un concentrateur esp32-c3-supermini.

Ce concentrateur  communique par uart avec un circuit imprimé equipé d'un esp32 wroom et d'un  emmeteur  lora pour la communication longue distance.

L'emmeteur lora tranmet les donnees d'un secteur vers le recepteur lora dans la salle de commande equipée d'un tp-link pour creer un reseau local. 

Sur ce tplink est branché un raspberry-pi avec mosquitto et nodered.

Le recepteur lora avec son esp32-wroom va transferer les commandes par mqtt.

L'ecran de controle est entierement geré par nodered 
