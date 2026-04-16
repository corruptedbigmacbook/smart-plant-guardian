# Architecture

The system follows a pipeline-based architecture:

ESP32 → MQTT → Python Backend → PostgreSQL → Django Dashboard

## Design Decisions

- MQTT used for lightweight messaging
- PostgreSQL for structured time-series storage
- Django for rapid backend + UI development
