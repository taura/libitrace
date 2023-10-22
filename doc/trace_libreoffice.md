libreoffice級のアプリをトレースする
===================================

libreoffice級のアプリで目当ての機能を実現するためにいじるべきソースファイルや関数を突き止めるには, 様々な困難がある.

 * 目当ての関数は大概の場合, gtk, glibなど, 自分でコンパイルしていない(デバッグシンボルのない)ライブラリからコールバックされるため, デバッガでmain関数から追っていっても途中で追うことができなくなる
 * もっともGUIのようなインタラクティブなアプリではそもそもmainから追っていく必要はない. gdb内でbreakpointを張らずに普通に立ち上げた後で, 入力待ちになったところでctrl-cでデバッガに制御を戻せばそこから追っていくことはできる. しかしこの場合でも, 入力待ちになっているのは, gtk/glibなど, 自分でコンパイルしていない関数なので, 実際に追っていくのは困難.
 * gtkとglibのデバッグシンボルパッケージ ( https://wiki.ubuntu.com/Debug%20Symbol%20Packages )と, ソースパッケージ(apt source ...)をインストールすれば, 大概のアプリはソースレベルで追跡することが可能にはなる
 * ここまでやれば例えば, 入力待状態から finish を繰り返して, gtk が callback を呼び出す箇所を突き止めるなどのことは可能だが, 結構根性がいる
 * このような追求を楽にするために必要なのは, gtk/glibなどのライブラリから, 自分でコンパイルした「任意の」関数が呼ばれたらそこでbreakしてくれる, というもの. そんなものがあれば, 一旦アプリを入力待ち状態にしておいて, それ以降, 「自分でコンパイルした「任意の」関数が呼ばれたらそこでbreak」という指示をした上でアプリケーションに何か入力をしてやることで, gtk/glibなどのライブラリから再び, アプリケーションに制御が戻った時点を突き止めることができる. この機能があれば少なくとも, どんなcallbackが呼ばれたかをデバッガ内で突き止めることは比較的容易にできる.
 * gccには-finstrument-functions という, 全関数の先頭で決まった関数 __cyg_profile_func_enter を読んでくれるというオプションがある. アプリケーションをこのオプションつきでコンパイルして __cyg_profile_func_enter にブレークポイントをはって実行すれば, まさに上記で言ったことができる.
 * これをやると, gnumeric, libreoffice などでgtkから呼ばれている関数を列挙することは比較的簡単にできた.
 * しかしこれができてもなお, 例えばセルに入力された式をパースしているのはどこか, みたいな問いに応えるには勘と根性がいる. 一個の式, 例えば, =3+4 を入力するだけでもwindow focus, key press, key releaseなど多数のcallbackが呼ばれる. key press, key releaseは=, 3, +, 4, \n のそれぞれに呼ばれるから, だいたい合計で10-15くらいのcallbackが起動される. なので, どのcallback内で式をパースしているのかもあまり確信は持てない. アプリケーションの動作を注意深く観測すると, おそらく '\n' キーが PRESS されたイベント内であろうという想像はつくし, 実際そのとおりではあるのだが, 確信は持てないまま追跡する必要がある. かりに '\n' の key press イベントが目当てのものだと思って追跡したとしても, 式をパースした瞬間に画面に変化が現れるわけではないので, どの関数内でパースしているかを知るのはそれなりに勘と根性がいる. 本来は, 式が入力されたらセルのデータを修正するわけだから, セルのデータがどこに格納されているのかが突き止められれば, そのセルのデータを見張りながら追跡することで所望の関数に辿り着けそうであるが, セルのデータの格納場所を突き止めること自体もそれないに困難である.
 * ここに至って, 困難の根本的な原因を振り替えると, 結局, 長い一連の動作のどこで目的の動作が行われているのかを知るのは困難であるという当たり前の事実(これは, GUIアプリだから特別困難というわけではない. GUIアプリでも一つのイベント内の処理が十分シンプルならこの方法で追跡可能だろうし, GUIアプリでなくても長くかつ, 関数名から想像し難いところで目当ての処理が行われていたらそこにたどり着くのは困難だろう).
 * それでもソースコードを当てずっぽうでfind, grepするというよりも, システマティックに「実際に実行された関数(やソースコード)を洗い出す」方法が欲しい. デバッガのようにそれをマニュアルでひとつひとつ見ていく方法は, ステップ数が増えると困難になっていくので, できれば実行された関数名やそのソースファイルを自動的に列挙するようなツールが欲しい
 * -finstrument-functions を使えばそれを自作することができるだろうし, そもそもそれに似た機能としてプロファイラ(やトレーサ)があるわけだから, それらを上手く使えば所望のツールができるのではないか.
 * そう思ってアレコレ試行錯誤した結果: サマリとしては,
  * gprof : 最近のものは使い物にならないバグがある. また, 共有ライブラリに対して無能
  * uftrace : いいツールで, これを使いこなせばだいたい欲しいものが手に入りそう. 実際gnumericに対しては上手く行ったのだが, libreofficeに対しては原因不明の不具合で上手く行かない. だが他のアプリでも有望な方法
  * -finstrument-functions + 自作ライブラリ : 付け焼き刃的機能はすぐに作れる. gnumeric, libreofficeに対して所望の動作をするものは作れた. 
 
gprof (失敗)
===================================

 * gprofはどの関数が何回呼ばれたかは正確に数えている
 * マニュアルには書かれていない機能として, mcontrol(0); mcontrol(1); という呼び出しによって, 途中で記録を off/on できる
 * アプリのソースをいじらなくてもgdbで走らせて, mainの先頭でbreakして
```
(gdb) p mcontrol(0)
(gdb) continue
```
とでもして入力待ちになったらCtrl-Cして
```
(gdb) p mcontrol(1)
(gdb) continue
```
とでもする. そしてセルに入力し終えてまた入力待ちになったらまたCtrl-Cして
```
(gdb) p mcontrol(0)
(gdb) continue
```
とでもして正常終了させれば, だいたいセルに入力している間だけを記録できる.
その後に,
```
gprof -C -b soffice.bin
```
とでもすればその間に実行された関数だけが出てくる.

これでうまく行く「はず」だが, 実際には上手く行かない.

 * gcc-5 以降はそもそもgprofは全くまともに動作しない. 何をやっても何も出てこない
 * gcc-4.9 はOKだが, そもそもgprofでは共有ライブラリ内は全く記録できていないようである.
 * 遠藤君情報:

  軽く調べただけですが，"gprofは共有ライブラリに対応していない"というような
  情報を複数見かけました．
  https://gcc.gnu.org/ml/gcc/2006-12/msg00690.html
  https://sourceware.org/ml/binutils/2007-04/msg00250.html

sprof
===================================

共有ライブラリをprofilingできるものとして, sprofがあるようだが上手く行かない模様.

 * 遠藤君情報:
  sprofについてもバグが報告されて放置されている気がします…．
  https://bugs.launchpad.net/ubuntu/+source/eglibc/+bug/462760


uftrace
===================================

かなりいい線いっているツール. gnumericでは期待通りの動作をする. libreofficeでは原因不明の失敗をする. 使い方は, gprofと同じく -pg でコンパイル. リンク時には -no-pie が必要. 実行時はuftraceコマンドで実行

```
sudo apt install uftrace
```

```
gcc -pg ... -o libxxx.so
gcc -no-pie -o a.out ... -lxxx
uftrace record ./a.out
uftrace report ./a.out
```
または以下で全トレースを表示可能.
```
uftrace live ./a.out
uftrace ./a.out
```
色々なオプションで動作を制御可能
 * --diable : 初期状態で記録しない
 * --trigger=関数名@trace_on : この関数が呼ばれたら記録開始
 * --trigger=関数名@trace_off : この関数が呼ばれたら記録やめ
以上を組み合わせると
```
 uftrace --disable --trigger=start_prof@trace_on --trigger=stop_prof@trace_off ./gnumeric-1.12-35
```
みたいにして起動し, アプリが入力待ちになったところで,
```
$ gdb ./gnumeric-1.12-35
(gdb) attach <pid>
```
とでもして,
```
(gdb) p start_prof()
(gdb) continue
```
とでもしてセルに何か入力. Ctrl-Cでまたデバッガに制御を戻して,
```
(gdb) p stop_prof()
(gdb) continue
```
とでもして正常終了させれば取りたいところだけが取れる.

Ubuntu 17.04ではgnumericではこれで無事, いい感じの動作をした.
Ubuntu 17.10では, 謎なsegmentation faultがおきる(原因追跡していない).

libreofficeの問題
----------------------
 * configure時にCFLAGS="-pg ..." CXXFLAGS="-pg ..." LDFLAGS="-no-pie" を指定するとリンクが失敗する.
 * LDFLAGS="-no-pie"を指定せずにリンクをした上で手動で
```
 gcc -no-pie -pg -Wl,-z,origin -Wl,-rpath,/home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program -Wl,-rpath-link,/home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program -Wl,-z,defs -fstack-protector-strong -Wl,-rpath-link,/lib:/usr/lib -Wl,-z,combreloc  -Wl,--hash-style=gnu  -Wl,--dynamic-list-cpp-new -Wl,--dynamic-list-cpp-typeinfo -Wl,-Bsymbolic-functions  -L/home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/workdir/LinkTarget/StaticLibrary -L/home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/sdk/lib  -L/home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program  -L/home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program  /home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/workdir/CObject/desktop/source/app/main.o -Wl,--start-group -Wl,--end-group -Wl,--no-as-needed -luno_sal -lsofficeapp -o instdir/program/soffice.binx
 ```
みたいにして手動でリンクすると, 一応リンクは成功する(前者でどのようなコマンドが実行されているかは調べていない)が,
```
uftrace record instdir/program/soffice.binx --calc
```
みたいに実行すると謎のsegmentation fault
```
namopa:libreoffice$ uftrace record instdir/program/soffice.bin --calc
warn:configmgr:2913:2913:configmgr/source/xcuparser.cxx:296: unknown component "org.openoffice.Office.UI.ReportCommands" in "file:///home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program/../share/registry/res/registry_en-US.xcd"
warn:configmgr:2913:2913:configmgr/source/xcuparser.cxx:296: unknown component "org.openoffice.Office.UI.DbReportWindowState" in "file:///home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program/../share/registry/res/registry_en-US.xcd"
warn:configmgr:2913:2913:configmgr/source/xcuparser.cxx:908: ignoring modify of unknown set member node "StarOffice XML (Base) Report" in "file:///home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program/../share/registry/res/fcfg_langpack_en-US.xcd"
warn:configmgr:2913:2913:configmgr/source/xcuparser.cxx:908: ignoring modify of unknown set member node "StarOffice XML (Base) Report Chart" in "file:///home/tau/public_html/lecture/dive_to_oss/projects/libre/libreoffice/instdir/program/../share/registry/res/fcfg_langpack_en-US.xcd"
child terminated by signal: 11: Segmentation fault
```
 * 追求するとしたらまずは, uftrace が子プロセスをどのように起動しているのかを調べてみるのが正解だろう. おそらく LD_PRELOAD なんかで mcount etc. をのっとって実行しているだけだろうから, それを理解すれば, uftraceなしで直接gdb内で実行できるだろう. そうすればsegmentation faultのデバッグもしやすい.
 * coreをはかせてデバッグということもできるとは思うが.
 
-finstrument-functions
===================================

 * gccの-finstrument-functionsを使うと自前でtracerみたいのを作ることができる.
 * CFLAGSとCXXFLAGSに-finstrument-functionsを追加するだけ. 
 * 各関数の先頭で __cyg_profile_func_enter, 終了間際に __cyg_profile_func_exitが呼ばれるようになる.
 * できたコマンドは普通に実行すれば特に何もしない
 * gdb内で実行して__cyg_profile_func_enter でbreakすれば, すべての関数にbreakpointをはったのと同じような効果がある.
 * 例えばこれだけでも, callbackして呼ばれた関数を見つけるのに役に立つ
 
-finstrument-functions でお手製トレーサ
===================================
 
 * cyg_profile_func_enter を定義した .so を作り, LD_PRELOADで挿入すれば, お手軽トレーサを作れる
 * 例えばそこで呼び出した関数のアドレスを記録しておく
 * プロセスのどのアドレスにどの共有ライブラリが張り付いているかは /proc/<pid>/maps (自プロセスのは /proc/self/maps) を見るとテキスト形式で見られる
 * ので, 両者を突き合わせれば各関数のアドレスから, それが格納されている共有ライブラリ名と, 共有ライブラリ内でのアドレス(ライブラリ内でのオフセット)を得ることができる
 * その両者を addr2line に放り込むと, ソースファイル名や行番号を得ることができる
 * 呼ばれた関数名だけでなくそのスタックトレースが欲しければ backtrace 関数を使うのもよい

libitrace
================

 * 上述の方針に基づいて試作したライブラリが projects/libitrace

https://doss-gitlab.eidos.ic.i.u-tokyo.ac.jp/tau/libitrace

```
LD_PRELOAD=libitrace.so COMMAND で起動
```
 * memory mapされた領域(*)に実行されたアドレスを書いていく
 * それとともに適当なところで /proc/self/maps をダンプする
 * その領域(*)を好きなところで読み出して/proc/self/maps のダンプと組み合わせて所望の情報を得る
 * どのアドレスを記入するか, 領域が溢れたときの方針など, まだ試行錯誤中

gnumeric + libitrace
================

```
./configure --prefix=/home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/inst CFLAGS="-O0 -ggdb3 -finstrument-functions" CXXFLAGS="-O0 -ggdb3 -finstrument-functions"
make install
cd /home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/inst/bin
LD_PRELOAD=../../../libitrace/libitrace.so ITRACE_INIT_STATUS=0 ./gnumeric-1.12.35 &
.. 窓が出て入力待ちになったら ...
## itrace_log_<pid>.dat itrace_maps_<pid>.dat というファイルができているはず
../../../libitrace/itrace_cmd start itrace_log_<pid>.dat
何かセルに入力
../../../libitrace/itrace_cmd stop itrace_log_<pid>.dat
アプリを操作して終了
../../../libitrace/itrace_show itrace_log_2949.dat
path /home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/inst/lib/libspreadsheet-1.12.35.so
resolving 680 addresses for /home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/inst/lib/libspreadsheet-1.12.35.so
 src /home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/src/application.c
  fun   101    32 gnm_app_get_app
  fun   213     1 gnm_app_clipboard_unant
  fun   495     1 gnm_app_display_dpi_get
  fun  1017     1 gnm_app_recalc
  fun  1036    22 gnm_app_recalc_start
   ...
```
なおこの出力は, 745行 (つまり異なる関数が745個)
namopa:bin$ ../../../libitrace/itrace_show itrace_log_2949.dat | wc
    745    2853   32263
```

これを" src"でgrepすると, 異なるソースファイル名が列挙できる(63ファイル).
```
namopa:bin$ ../../../libitrace/itrace_show itrace_log_2949.dat | grep " src"
 src /home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/src/application.c
 src /home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/src/auto-format.c
  ...
```
このファイルのリストを眺めると src/expr.c, その中の関数名を眺めると gnm_expr_eval などの関数も見つかる. gnumericの場合, find で見つかる .c ファイルの数が242なので, これならglobalsなどでも見つけられるのではという気もする(ツールがきちんと動けばこちらのほうが見通しがいいのは確か).

gnumeric + uftrace
================

入力待ちになったら記録開始, 入力したら記録終了, ということをするために以下のようなプログラムを作っておく(start_stop_prof.c) .
```
void start_prof() { }
void stop_prof() { }
```

```

```

```
namopa:gnumeric-1.12.35$ ./configure --prefix=/home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/inst CFLAGS="-O0 -ggdb3 -pg" CXXFLAGS="-O0 -ggdb3 -pg" LDFLAGS=-no-pie
make
make install
cd /home/tau/public_html/lecture/dive_to_oss/projects/gnumeric-1.12.35/inst/bin
uftrace record --disable --trigger=start_prof@trace_on --trigger=stop_prof@trace_off ./gnumeric-1.12.35 

```

libreoffice + libitrace
================

