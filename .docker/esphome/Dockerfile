FROM esphome/esphome

RUN pip install esptool

WORKDIR /data

COPY . .

CMD ["esphome", "run", "test.yaml"]