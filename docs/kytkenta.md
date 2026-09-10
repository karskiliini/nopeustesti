# Kytkentä

Kuvallinen versio: https://claude.ai/code/artifact/46e39b36-81aa-464a-af4a-f65cea305ee2

## Yksi nappiyksikkö (valaistu kupunappi)

Napin sisällä on kaksi erillistä osaa: mikrokytkin (liittimet COM, NO, NC,
merkinnät valettu kytkimen kylkeen) ja lamppu (liittimet + ja −).

```
 Arduino Dn (nappi) ────────────────────► NO ┐
                                             │ mikrokytkin
                                 (tyhjä) NC  │
                                             │
                          ┌──────────── COM ┘
                          │  musta hyppyjohto
 Arduino Dm (lamppu) ───► + lamppu − ─┘
                          │
                          └── yhteinen maa ► seuraavan napin COM … ► Arduino GND
```

* Nappijohto Arduinosta NO-liittimeen. NC jää tyhjäksi.
* Lamppujohto Arduinosta lampun plussaan. 5 V:n lamppuosassa on etuvastus
  valmiina, ulkoista vastusta ei tarvita.
* Lampun miinus ja kytkimen COM yhdistetään (musta hyppyjohto), jolloin
  napista lähtee yksi maajohto. Maa saa ketjuttua napista toiseen, mutta
  ketjun pään on päädyttävä Arduinon GND-pinniin.
* Ulkoisia vastuksia nappeihin ei tarvita: sketsi käyttää sisäistä
  ylösvetoa. Levossa pinni lukee HIGH, painettuna LOW.

## Koko laite

| paikka | lamppu    | lamppu + | nappi NO | tila 10.9.2026                   |
|--------|-----------|----------|----------|----------------------------------|
| 1.     | vihreä    | D6       | D0 ?     | lamppu ok, nappi ei näy          |
| 2.     | sininen   | D7 ?     | D1 ?     | lamppu ei syty, nappi ei näy     |
| 3.     | keltainen | D13      | D2 ?     | lamppu ok, nappi ei näy          |
| 4.     | punainen  | D12 ?    | D3 ?     | lamppu ei syty, nappi ei näy     |
| näyttö | TM1637    | CLK D9, DIO D10, VCC 5V, GND |   |                    |
| summeri| passiivinen | D4 / GND |        | valinnainen                      |

Yhdeksän johtoa Arduinoon: 4 nappia, 4 lamppua, 1 maa. Kysymysmerkillä
merkityt ovat config.h:n oletuksia, ei vielä mitattuja.

## Vianhaku

Mittauksessa yksikään nappi ei muuttanut mitään pinniä, ei levossa eikä
painettuna. Näin käyttäytyy:

* kytkin, jonka johdot ovat NO- ja NC-liittimissä ja COM on tyhjä
* nappi, jonka maaketju ei pääty GND-pinniin

Jos maa olisi NC:ssä ja nappijohto COM:ssa, pinni näkyisi levossa
alasvedettynä (`tools/sercmd.py q` → DOWN). Sitä ei nähty.

Tarkistusjärjestys:

1. Musta hyppyjohto COM-liittimessä, nappijohto NO-liittimessä, NC tyhjä.
2. Maaketjun pää Arduinon GND-pinnissä.
3. `tools/sercmd.py q 1.5` nappi pohjassa: napin pinni vaihtuu `-` → `DOWN`.
4. Pinnit config.h:n CHANNELS-taulukkoon järjestyksessä vihreä, sininen,
   keltainen, punainen. `./flash.sh`.
5. Sininen ja punainen lamppu: + -johto pinniin, − samaan maaketjuun.
   Vilkutus `tools/sercmd.py 7 1`.
