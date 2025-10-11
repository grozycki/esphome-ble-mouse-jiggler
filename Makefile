.PHONY: test clean prune

test:
	@echo "Running compilation pipeline..."
	@docker compose up --build --exit-code-from compile-espidf

clean:
	@echo "Stopping containers and removing local build artifacts (cache is preserved)..."
	@docker compose down --remove-orphans
	@rm -rf .esphome

prune:
	@echo "Performing a full cleanup: stopping containers, removing volumes (cache), and images..."
	@docker compose down --volumes --remove-orphans --rmi all
	@rm -rf .esphome .cache
