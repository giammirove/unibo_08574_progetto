## PandOS Plus 
### Compilare il kernel e i tests
```bash 
mkdir -p build && cd build && cmake .. && make
```

### Usare l'emulatore con il kernel compilato
Nella cartella `machines` e' presente un file di configurazione gia' compilato
con i vari tests e dispositivi (`config_machine.json`).
Comandi per eseguire l'emulatore:
```bash
cd machines && uriscv-cli
```
Nota:
gli indirizzi presenti nella configurazione sono relativi, quindi affinche' l'emulatore
funzioni e' necessario eseguirlo nella stessa cartella del file di configurazione

