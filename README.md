# push_to_talk_webapp
webapp for streaming audio to a server running on a PC

## On the Laptop

- 1) Run the `webserver.py` script inside the folder `/server`:
  ```
  python3 webserver.py
  ```
- 2) Inside the `webpage` folder run:

  ```
  python3 -m http.server 8080
  ```

## On the smartphone
- 1) Connect to the same Wi-Fi as your PC.

- 2) Open your browser and go to:
  ```
    http://YOUR_PC_IP:8080
  ```
  YOUR_PC_IP is printed when you run the `webserver.py`