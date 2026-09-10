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
| 1.     | vihreä    | D6       | D2       | mitattu 10.9.2026                |
| 2.     | sininen   | D5       | D11      | mitattu 10.9.2026                |
| 3.     | keltainen | D13      | D1       | mitattu 10.9.2026                |
| 4.     | punainen  | D7       | D0       | mitattu 10.9.2026                |
| näyttö | TM1637    | CLK D9, DIO D10, VCC 5V, GND |   |                    |
| summeri| passiivinen | D4 / GND |        | valinnainen                      |

Yhdeksän johtoa Arduinoon: 4 nappia, 4 lamppua, 1 maa. Kaikki pinnit on
mitattu 10.9.2026. Nappien vika oli irti ollut maajohto.

Mikrokytkimen merkinnät (kuvattu 10.9.2026): COM1 alareunassa, NO3
oikealla alhaalla (musta hyppyjohto lamppuun), NC2 oikealla ylhäällä,
tyhjä. Lampun pinnit on
mitattu. Summeri siirtyy pinniin 4 (config.h), koska 5 on sinisen lamppu.

Mikrokytkimen merkinnät (kuvattu 10.9.2026): COM1 alareunassa, NO3
oikealla alhaalla (musta hyppyjohto lamppuun), NC2 oikealla ylhäällä,
tyhjä.

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
5. Lamput on mitattu 10.9.2026: vihreä 6, sininen 5, keltainen 13,
   punainen 7. Vilkutus tarvittaessa `tools/sercmd.py 7 1`.
