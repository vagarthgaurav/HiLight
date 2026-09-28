# Broker hardening

Files here go to `/home/vagarth/HiLight-mosquitto/` on the server. `passwd` is
never committed.

1. Copy `mosquitto.conf` and `acl` over, and `mkdir data && chown 1883:1883 data`.
2. Create users (the `device` password must equal `MQTT_PASSWORD` in `src/secrets.h`):

       cd /home/vagarth/HiLight-mosquitto && touch passwd
       docker run --rm -it -v $PWD:/w eclipse-mosquitto:2 mosquitto_passwd -c /w/passwd device   # first user only (-c creates)
       docker run --rm -it -v $PWD:/w eclipse-mosquitto:2 mosquitto_passwd /w/passwd cloud
       docker run --rm -it -v $PWD:/w eclipse-mosquitto:2 mosquitto_passwd /w/passwd admin
       chmod 600 passwd && sudo chown 1883:1883 passwd

3. Redeploy the stack with `docker-compose.yml` (drops the host port mappings).
4. Rollout: lamps on the old firmware connect anonymously and will be locked
   out the moment `allow_anonymous false` takes effect, and you can no longer
   OTA them over MQTT. "Alexandra & Gabriel" and "Jutta & Patrick" also still
   need a USB reflash anyway (new partition table, see the firmware review
   notes). So: flash every lamp with the new firmware first, and only then
   switch the broker to authenticated-only. Until then, keep the old
   broker running and do steps 1-2 and 5 in advance.
5. Set `MQTT_USERNAME=cloud` / `MQTT_PASSWORD` in the HiLight-cloud stack, and
   Home Assistant's MQTT integration to `admin`.
