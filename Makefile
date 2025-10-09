.PHONY: test clean

test:
	@echo "Running tests with docker compose..."
	@docker compose up --build --abort-on-container-exit

clean:
	@echo "Cleaning up..."
	@docker compose down --volumes --remove-orphans
	@rm -rf .esphome .cache
