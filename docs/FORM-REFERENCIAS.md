# FORM: las referencias de Juan (2026-11-19)

Fórmulas (bytebeat / floatbeat) que Juan pasó como referencia de lo que FORM debería poder sonar. Llegaron por chat:
el formato se comió algunos `*` (donde dos factores quedan pegados, es una multiplicación). Debajo de cada una, lo que
aporta. Al final, el diseño que sale de todas.

**Lo que le falta a FORM hoy** (eng_form.c, FORM_STATE): las fundamentales son casi siempre una permutación de 0 3 5 7
semitonos (la misma progresión en los 22 estados); melodías y fundamentales en semitonos, no en grados (cambiar de
escala solo las corre a la escala más cercana: la misma forma); las melodías son la misma pentatónica 0 3 5 7 10 12
rotada (SEED / EVOLVE rotan, no inventan); acordes siempre fundamental + 3 + 5 + arriba, sin inversiones ni conducción
de voces; progresión de 4, idéntica cada ciclo.

## 1. Acordes rasgueados en 3-3-2 (la base de FORM; "037a05ac")

```
t||(d="037a05ac",readD=(i,ch)=>parseInt(d[i4+ch],36),w=0,ech=Array(r=12288).fill(0)),M=t/48e3,T=t,t=1.3,(t>>6)%(2**"4233"[t>>15&3]32)||(a=0),a++,k=tanh(sin(a*.216+TPI/512)/(a/256+1)4),c=Array(4).fill().map((_,i)=>tanh(sin(t2**(readD(t>>17&1,i)/12)/128PI)(1+t/8192%4))/3*((3-(t-i700)/8192%8%3)/3)**3min((t-i700)/8192%8%350,1)).reduce((a,b)=>a+b),t%2**((t>>15&-t>>12&7)-1)?ma=ma:ma=k+c*min(a/4096,1)2,ot=ech[T%r]=ma+ech[T%r]/4,w=w(q=t/262144%1)+ot(1-q)
```
Dos acordes de 4 (m7 0-3-7-10, cuartal 0-5-10-12) alternando; rasgueo (cada voz 700 muestras después); ritmo
modular `%8%3` (3-3-2) con envolvente al cubo; tanh con drive que respira; bombo redisparado por "4233"; sample & hold
de tasa cambiante (glitch rítmico); eco; pasabajos que barre.

## 2. House: voicings abiertos con nota común, sidechain

```
t||(hp=lp=[0,0],ech1=ech2=Array(ez=12288).fill(0),DELAY_DECAY=1.5),
s=17,p=[1.042t,1.043t],w=t>>9&31,v==>p[][.502,.4,.3,.448][3&t>>s],v2==>p[][.75,.6,.452,.67][3&t>>s],v3==>p[][.896,.796,.75,.896][3&t>>s],v4==>p[][1.126,.896,.896,1.126][3&t>>s],
ch==>Array(2).fill(0).map((__,i)=>((v(i)/2&w)+(v2(i)/2&w)+(v3(i)/2&w)+(v4(i)/2&w))*min(32-t/512%32,1))[],
b=sin((y=v(0)PI/256(1+(t>>13&1)))+sin(y)2(1-t/8192%1)),
lp=lp.map((,i)=>ch(i).01+lp[i].99), hp=hp.map((,i)=>(lp[i]-ch(i))/64),
chout1=ech1[t%ez]=hp[0]+ech1[t%ez]/DELAY_DECAY, chout2=ech2[t%ez]=hp[1]+ech2[t%ez]/DELAY_DECAY,
k==>tanh(sin(cbrt(t%16384)**.1512+(random()-.5)(kenv=(1-t/16384%1))50)*2)*kenv7, h=(random()2-1)/4(1-t/8192%1)**5, s==>tanh(((t*sin(t/2.5|0)+(40)&255)/128-1)(1-t/16384%1)*5(t>>14&1)*2)/2,
mast==>k()+((?chout1:chout2)+b/2+h+s())min(4-kenv4,1), [mast(0),mast(1)]
```
Acordes como relaciones: m9 abierto (1 1.5 1.78 2.24), add9, quinta + décima, mayor con décima; fundamentales
i - bVI - bIII - bVII; la voz de arriba (.896) común a los cuatro (conducción de voces). Sierra enmascarada por bits que
barre (`fase & w`); desafinación estéreo 1.042 / 1.043; corte rítmico al final del ciclo; pasa altos en los acordes;
bajo FM con realimentación; bombo con barrido y click, hat a la quinta, caja bytebeat en 2 y 4; sidechain del bombo.

## 3. Secuencia de intervalos justos, fase en raíz

```
T=t,t/=4,max(min(sin(sqrt(T/6*[1,1,1.2,1.5,1,1,1.2,1.1][t>>14&7]&63)4)/((t>>(t&8192?t&16384?2:3:4)&255).1)+atan(sin([1,1,1.2,1.5,1,1,1.2,1.1][t>>14&7]T/6PI/128+4E3/(t%8192))8)/2,1),-1)/2+(random()(-t>>3&255)/512+random()(-t>>6&255)/256(t>>13&1))/2-.25
```
Multiplicadores 1 1.2 1.5 1.1 por beat (justos); `sqrt(fase)&63` (timbre que barre en el ciclo); desplazamiento de bits
por sección (la figura se acelera o se frena); click de caída de pitch `4E3/(t%8192)`.

## 4. Generativa larga

```
v=5E4/(1E4+t%4E4)(B=sin)((t%4E4)**.250),(c==>v=A(v))((=>v+=2B(random())/1.0002**((t+4E4)%8E4))((_=>v+=A(B(t3)/1.0002(t%2E4))/2)(c(X=t/1250,v+=(A=tanh)(B(t/4E4PI2**((C=int)(X<<X^(X>>3|X>>6))%4)-6)**5B(tPI/200(1+C(2*(X>>5|X>>7)/(1+C(X>>X)))%8))B(tPI/25)**3100+B(tPI/800)*25),v/=2))))
```
Notas por operaciones de bits sobre un tiempo lento (`X<<X^(X>>3|X>>6)`): caótico con estructura; proporciones que
evolucionan en ciclos largos; cadenas de tanh con realimentación.

## 5. Bytebeat: melodías en enteros, ratchets

```
[(M=x=>(t*(y=x?1.01:1)(t>>9|1932555120>>(a=t>>13&7))&128^(ty>>2)(t>>11|58132275>>a)&191)/255-.75+(X=sin)(8cbrt(b=t%(16384/'11111111111144222211111122223389'[31&t>>13])))1200/b+(tan(tyPI/1024)(1&-t>>11)((3&t>>12)>0)/1.5400/(c=t%4096)&1)+(tX(t>>4)&255&-t>>6)(1&t>>(x?11:14))*(d=1&t>>19)/191+(e=random)()400/cd+(e()+X(t<<3)/3)*4E3/(t%524288))(0),M(1)]
```
Melodías empaquetadas en enteros de 32 bits; redisparo del bombo por string (2 4 3 8 9: redobles, ratchets);
desafinación estéreo 1.01; zumbido de tan().

## 6. Arreglo que se arma

```
A=4096,D=219,F=255,G=256,H=512,J=A2,B=t%A,a=tanh(atan(sin(6cbrt(B))5(1-B/A))/2+(random()(300/B))-(random()(300/B)))(0x84450441>>(31&t>>12)&1),C=[0,0,-4,5],S=(a,s)=>t2(a[t>>s&3]/12),I=t*[1,1,.8,1.5][3&t>>16],atan(tanh(a/(1-t%D/D)/250*(t<D)+a*(t>D)+(tsin(t>>1)&F)(1/(t%(J/'11222212'[7&t>>13])))-(tsin(t/1.0001>>1)&F)(1/(t%(J/'11222212'[7&t>>13])))+(1&t>>18)(random()-.5+((t2.25&F)/G)-((t2&F)/G))/(1-t%D/D)/196+((((tsin(t/1.75>>2)&F)-((tsin(t/1.751>>2)&F))(1-t%J/J))/H)(1&-t>>13)(1&t>>14))(t>D2)+((I*(t>>2^t>>13)&F)/H-.25)(t>D2)+((I*(t>>2&t>>13)&F)/H-.25)(t>D4)+(sin(A*'1213121412131278'[15&t>>13]/(t%A))(0x75d7>>(t>>12&15)&1))(1-t%A/A)/2*(t>D*4)+((acos(cos(S(C,16)PI/H))/5+acos(cos(S(C,16)PI/510))/5)-.5)(t>D6))*2)*90-128
```
Bombo como máscara hex (0x84450441); redobles '11222212'; los elementos entran de a uno (t>D*2, *4, *6); phasing por
desafinación mínima (t/1.0001); triángulos con acos(cos()).

## 7. La melodía cambia por sección, riser

```
t/=5.5125,a=t*((t&4096?59392>t%65536?7:t&7:16)+(1&t>>14))>>(3&-t>>(t&2048?2:10))|t>>(t&16384?t&4096?10:3:2),b=a/3%(85+1/3)+(61440>t%65536?44583.5284:0)/(t&4095)/2%128,c=(256>t%1024?4:0)trandom()|t>>0,d=(256>t%512?4:0)trandom()|t>>0,e=14336>t%16384?c:d,f=e/8%32+b%190,126976>t%131072?f:f+44583/(-t&4095)/2%128
```
El multiplicador y el desplazamiento de la melodía cambian por sección; barrido inverso antes del cambio (build).

## 8. Ritmos largos con fills

```
t/=4,a=t&219?t*(t&214?7:5)(3-(3&t>>11)+(3&t<<8)):t(t&214?7:5)(1+(3&t>>10)+(3&t<<7)),b=t(t&214?7:5)(3-(3&t>>(t&219?10:9))+(3&t>>(t&218?4:8)))>>(3&-t>>(59392>t%216?t&4096?2:16:2)),c=a/32%32+b/4%32,t&219?random()((-t>>'6554555455545544555455545554554365545554555455445554555455545543'[(t>>11)%64])%64+64)%102+(51200/(t%4096)&64)%180+c:t&2**18?random()((-t>>'65555554555555545555555455555544655555545555555455555554555554536555555455555554555555545555554465555554555555545555555455554321'[(t>>11)%128])%64+64)%102+(30200/(t%(t&8192?4096:6144))&64)%180+c:random()((-t>>'65555554555555545555555455555544'[(t>>11)%32])%64+64)%1+(30200/(t%(t&8192?3072:4096))&64)%180+c
```
Arpegios por multiplicadores 5 / 7 (cuartas, quintas) variando por bits; strings de 64 / 128 dígitos: el largo de la
envolvente de cada golpe (articulación), con fill al final ("4321"); la variante cambia por macro-sección (t&2**18).

## 9. Progresión escrita, voicings de 5

```
(t/=6)||(C=[[1.5873,4,4,4.7568,3.1748],[1.4983,5.3392,4.4896,3.5632,2.9964],[1.4983,5.3392,4.4896,3.7752,2.9964],[1,3.5632,4.7568,4,2.9964],[1.5873,4,4,4.7568,3.1478],[1.4983,5.3392,4.4896,3.5632,2.9964],[1,5.3392,4.4896,3.5632,2.9964],[1,5.0396,4,2.9964,2.9964]],m='ÿ@@ŀ@@Ĥ@@ÿ@@ê@Øê@ÿê@ØÀÀ@@@Ø@@êÿ@@ċ@ÿÿ@@@@Ĥ@@ċ@@ÿÿ@ċÿÿÿê@Ø@@êê@êÿÿÿê@@ØØ@ê@Ĥ@@ÿ@@ê@@ÿÿ@@@@@@@@õõõ'),y=2t,z=c=>(M=(m.charCodeAt(w=t/800%96)-64)t/(2048+c2),(C[s=int(t/9600%8)].map(e=>(32>w&&1<<w&0xC007307||(32<=w&&64>w)&&1<<w&1929842288||64<w&&1<<w&0xE70C400)&&sin((ye/(128+c/4)%2-1)9PI)/2).reduce((a,b)=>a+b)+sin((M%2-1)**9PI+sin(MPI2)/2))*(k=t/4800%1).5+sin(k**.2200+c/4)(1-k)6+(R=()=>random()-.5)()sin(t6)max(1-t/800%3%2,0)5+(t/4800&1&&sin(k.4300+R()*k2400-c/3)(1-k)**7)+R()(t/2400&1&&1-k)**21.2),[z(-1),z(1)]
```
8 acordes de 5 voces en temperamento igual (voicings abiertos de dos octavas y media); melodía en caracteres con
máscaras de 32 bits por sección (fraseo); lead FM `(M%2-1)**9` (pluck); desafinación estéreo +-1; bombo k**.2.

## 10. Grados apilados, progresión por bits (la clave)

```
C=0,r=(d=12e3)3,b=t/d%768,B=(b%640+(b/4&64))%192-64,P=s=>(1+(s+1)/8|0)2(((8-s%73)%11+11)%11)/3**(((5-s%72)%7+7)%7)/12212e3/d,(T=t)||(M=[],A=s=>Array(s).fill(0),L=(s,t,i)=>(f=t+t/(1+i),y=C++,z=C++,M[y]??=0,M[z]??=0,M[y]+=i(s-M[y]+f*(M[y]-M[z]))||0,M[z]+=i*(M[y]-M[z])||0),R=(s,t,i,n,b)=>(M[c=C++]??=A(r),j=T%r|0,f=s,o=0,t.forEach(s=>(f+=M[c][Q=(r+Ts[2]+s[3])%r|0]s[1],o+=M[c][Q]s[0])),M[c][j]=b(f)i,s(1-n)+on)),t?x+=P(b<256||b>512?(b^b/8+3)%12:(b^b/4+(b/6|1))%16%12)/4:x=0,S=L((abs(2x%1-.5)-.25)min(b/64,2),.6,.25)1.4+[3,5,7,9].map((s,i)=>(U=1-(max(0,B+64)%128/123|0))(abs(tP(D=s-((b<0?64-abs(b)%64:b)/8|0)%4)/2%1-.5)-.25)2+(0==i&&U&&abs(tP(D)/8%1-.25))/1.2(b%128<64&&b>64?1+(1&b):1)).reduce((s,t)=>s+t/2)||0,[[[2,2,1.02,1e4],[1.3,1.8,.98,7e3],[1,1.5,.98,6e3]],[[2,2,.98,1e4],[1.3,1.8,1.02,7e3],[1,1.5,.98,6e3]]].map((s,i)=>R(L(2S,.5,.01+min(b/128,1)*2.8),s,1.6,.4,s=>atan2(-L(s5,.01,.3),PI2)/3.5)).map(s=>s/1.5+(B>=0&&sin(sin(t/40)/4+140/(10+1e2(e=(t+d*(B/3&1))/(2048*(1+(((B+3)/64)+1&1)))%((d/512)(1+(((B+3)/64)+1&1))))**1.2))/e/4+1.9sin(t/16<<t/7)/(((d/64)/(32*(1+((B+2)/64&1)))+t/8192)%((d/64)/(16*(1+((B+2)/64&1))))).8/24+(B/64&1)*sin(t/32<<t/6)/(b/64%1).7*(1-b/64%1)**5/250)).map(s=>s/1.5);
```
P(s): un grado de la escala a frecuencia por aritmética modular (la escala es una función); acordes por grados
apilados [3,5,7,9] (tercera, quinta, séptima, novena EN LA ESCALA) que rotan (D=s-(b/8)%4: inversiones solas);
progresión por operaciones de bits sobre el compás (b^b/8+3, b^b/4+...): diatónica, cambia por sección; filtros de
estado variable, reverb de peines / allpass con taps distintos por canal; el filtro se abre con el arreglo.

## 11. Nibbles, arpegio con silencios, pulso que respira

```
p=v=>2**(v/12),z=12*(t>>18&1),
w=t/32768%1, w2=t/16384%1,
r=s=>(t>>13&1&&p(0xA7305730>>(t>>14&28)&15)ts/11&32),
e=(sin((t>>13&1&&p(0xA7305730>>(t>>14&28)&15)*t/22&32))*200),
a=(b,c)=>atan(tanh((((96abs(sin(tb/50p([0,3,7,12,,19,,7][t>>11+(t>>13&3^t>>14&1)&7]+z)))**(5sin(tPI2/158292)+10)+(164*(1-t%8192/8192)(1&-t>>13)(sin(600cbrt(t&16383)**.2)))+64(t>>13&1)*(1-w2)sin((tb/40%[10,10,10,10,10,10,10,10.5][7&t>>14]|0)**3)+(p([0,3,7,12][t>>11&3]+z)t/11%16)+(r(c/2)-r(1.01c))*2)+e)-128)/128)),
[a(.995,1),a(1.005,1.5)]
```
Melodía en nibbles (0xA7305730: 10 7 3 0 5 7 3 0), una octava arriba cada 2^18; arpegio con silencios
[0,3,7,12,,19,,7] con el índice mezclado por XOR sobre el compás; ancho de pulso que respira
(abs(sin)**(5 sin(lento)+10)); lead casi sincronizado ([10 .. 10.5]); estéreo +-0.5 %.

## El diseño que sale (FORM nuevo, de cero)

| Capa | Opciones |
|---|---|
| Armonía | grados apilados en la escala (3 5 7 9, ref. 10), voicings abiertos (m9, add9, 5 + 10, sus, cuartales: 2, 9), inversiones por conducción de voces con nota común (2), progresiones por familia (i bVI bIII bVII, ii V I, vamp modal, bajo descendente, por quintas) o por bits sobre el compás (10) |
| Melodía | arpegios con patrón y silencios (11), intervalos justos (3), melodías por bits / nibbles / enteros (4, 5, 11) |
| Ritmo | grilla hex (6), familia modular 3-3-2 y otras (1), ratchets y redobles por string (5, 6), articulación y fills por string (8), desplazamiento por sección (3, 7) |
| Timbre | máscara de bits que barre (2), raíz de la fase (3), FM con realimentación (2, 9), pulso que respira (11), saturación que respira (1), phasing y desafinación estéreo (2, 6, 11) |
| Forma | entradas de a una (6), riser antes del cambio (7), mutación por sección (EVOLVE), ciclos largos (4) |
| Mezcla | sidechain del bombo (2), pasa altos en los acordes (2), eco por canal (1, 2) |

Cada estado: una combinación de capas, no un string de notas. A / B / MORPH mezclan caracteres; SEED y EVOLVE generan
(no rotan). Lo que decide se decide por paso o por sección: el costo por muestra, el de hoy.

## 12. Growl dubstep: formantes por sincronismo, wobble trapezoidal

```
sr=48E3,ts=t*abs(150/(sr*120)),v=(t,p,te)=>sin(max(-PI/2,min(PI/2,asin(sin(t*PI+p*PI))*te)))/2+.5,tt=2**(parseInt('5 5 5 5 5 5 5 5 55  5 5 55  55  5 5 55  55  55  55  5 5 5 5 008855  55  5 5 55  55  5 5 5 5 008855  55  5 5 008855  5588cccc0000'[ts*16&127],36)/12-5/12)*t*256*55,(sin(t/sr%(128/tt*t)*((v(ts/2,2,1.5)*140+50)*256)*PI/32)+sin(t/sr%(128/tt*t)*(v(ts/2,1,1.5)*3080+2200)*PI))/2*(1-tt/128/sr%1)||0
```
Dos senos a frecuencias de formante (grave ~50-190, agudo 2200-5280) que reinician la fase cada ciclo de la nota (hard
sync: el tono de la nota, la vocal de los formantes); wobble trapezoidal (seno recortado: mesetas en los extremos), cada
formante con su fase (la vocal cambia, no solo el brillo); notas por semicorchea en un string con silencios y saltos de
quinta y octava; envolvente por ciclo de nota. Barato: dos senos y un reinicio de fase.

## La capa de carácter (Juan: dubstep, timbres complejos y distorsionados)

WOBBLE (LFO sincronizado 1/4 1/8 1/8T 1/16 con patrón, sobre filtro e índice FM; forma de seno a trapezoide a cuadrada),
REESE (sierras desafinadas, phasing), GROWL (ref. 12: formantes por sincronismo), SCREECH (FM de índice alto), SUB (seno
limpio debajo de la distorsión); cadena de distorsión: wavefold, tanh, bitcrush, sample & hold. Parámetros de carácter
que cambian la fórmula (pliegue, índice, wobble, desafinación, formantes), en lugares libres de la parte.

Decidido (Juan, 2026-11-19): la armonía nueva también en los 22 estados actuales; el MORPH compone (lo discreto paso a
paso por probabilidad, lo continuo mezclado).
