◆概要

秋月ジャンクマトリクスLEDを使用したRSSニュース表示機

マトリクスLEDモジュールは７セットを使用

線部３セットに「ニュースソース」、後部４セットにニューストピックスを表示

◆参考資料

https://akizukidenshi.com/img/contents/kairo/%E3%83%87%E3%83%BC%E3%82%BF/%E8%A1%A8%E7%A4%BA%E8%A3%85%E7%BD%AE/LED%E9%9B%BB%E5%85%89%E6%8E%B2%E7%A4%BA.pdf

◆参考情報１

https://cba.sakura.ne.jp/kit01/kit_464.htm

◆参考情報２

https://x.com/YI_Studio/status/1842111760213663799

https://x.com/YI_Studio/status/1845173685524758563

https://x.com/YI_Studio/status/1845208811319308641

https://x.com/YI_Studio/status/1845302071962239189

https://x.com/YI_Studio/status/1881400723067621603

https://x.com/YI_Studio/status/1887897703587398120


◆ピンアサイン

マトリクスLEDパネルと接続するGPIOは以下の通り。

　SIN1      12

　SIN2      13

　SIN3      14

　CLOCK     15

　LATCH     16

また、表示の制御に使うGPIOは以下の通り。

・スライドの速度設定（必須）

　可変抵抗の電圧値をADCで取得し速度を決定

　VOL       34

・スライド表示の一時停止（省略可）

　BTN        4

・Wi-Fiデータ受信インジケーター（省略可）

　LED        2

◆回路図

https://github.com/Y-Ishioka/esp32_matled-junk-2line-pub/esp32_matled-junk-2line-pub.jpg

◆免責

　著作者は，提供するプログラムを用いることで生じるいかなる損害に関して，一切の責任を負わないものとします．

　たとえ，著作者が損害の生じる可能性を知らされていた場合も同様に一切の責任を負わないものとします．
