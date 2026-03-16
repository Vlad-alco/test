Если Вариант A не даст точного результата — пересчитать также **время этапов** по реальному расходу:

```cpp
// KSS_SPIT
float speedMlMin = valve_head_capacity;  // мл/мин
golovyTargetTime = (headVol / speedMlMin) * 60;  // секунды
accumSpeed = speedMlMin * 60;  // мл/час

// KSS_STANDARD
float dutyCycle = (headOpenMs * koff) / (headOpenMs * koff + headCloseMs);
float speedMlMin = valve_head_capacity * dutyCycle;
golovyTargetTime = (headVol / speedMlMin) * 60;
accumSpeed = speedMlMin * 60;
```

**Плюсы B:** Полная согласованность — время и объём совпадают.
**Минусы B:** Время этапов изменится, нужно заново калибровать capacity.

### Изменённые файлы
- **ProcessEngine.cpp**: функция `handleGolovy()` — новый расчёт `accumSpeed` для каждого подэтапа

---

