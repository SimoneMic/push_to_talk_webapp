# push_to_talk_webapp
webapp for streaming audio to a server running on a PC

## On the Laptop
- 1) Inside the `webpage` folder run:

```
python3 -m http.server 8080
```
- 2) Run the `server.py` script inside its folder:
  ```
  python3 webserver.py
  ```

## On the smartphone
- 1) Connect to the same Wi-Fi as your PC.

- 2) Open your browser and go to:
  ```
    http://YOUR_PC_IP:8080
  ```
