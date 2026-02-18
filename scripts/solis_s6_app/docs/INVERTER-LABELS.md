# Inverter Labels

In this project we use the following display names for inverters:

| Label    | Device / role                | Notes |
|----------|------------------------------|--------|
| **Solis1**  | Solis S6 11.4 kW             | Modbus TCP at 10.10.53.16:502 (this app talks to it). |
| **Solark1** | New board we are making now  | Same board as Solark2; **Modbus ID 0x01** (primary) on that board. |
| **Solark2** | Second Modbus ID on same board | **Modbus ID 0x02** on the same board as Solark1 (slave/second inverter). |

- **Solis1**: Solis S6 hybrid; the app connects to it and shows data/controls under this label.
- **Solark1** and **Solark2**: One physical board (Battery-Emulator + Solark RS485). Same IP (`SOLARK1_HOST`); Solark1 = Modbus unit 1, Solark2 = Modbus unit 2. Configure `SOLARK1_HOST` when the board has an address; use `SOLARK1_MODBUS_UNIT` (default 1) and `SOLARK2_MODBUS_UNIT` (default 2) for the two IDs.

Config: `config.py` defines labels, `SOLARK1_HOST`, `SOLARK1_MODBUS_UNIT`, `SOLARK2_MODBUS_UNIT`, and `CURRENT_INVERTER_LABEL` (default `Solis1`).
