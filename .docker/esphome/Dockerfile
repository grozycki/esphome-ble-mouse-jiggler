FROM esphome/esphome:stable

#RUN pip install esptool

WORKDIR /data

COPY . .

CMD ["esphome", "run", "test.yaml"]
