; RUN: opt < %s -passes=loop-simplify,lcssa -verify-loop-info -verify-dom-info -S | FileCheck %s

; LCSSA should work correctly in the case of an indirectbr that exits
; the loop, and the loop has exits with predecessors not within the loop
; (and btw these edges are unsplittable due to the indirectbr).
; PR5437
define i32 @test0(i1 %arg) nounwind {
; CHECK-LABEL: @test0
entry:
  br i1 %arg, label %"4", label %"3"

"3":                                              ; preds = %entry
  ret i32 0

"4":                                              ; preds = %entry
  br i1 %arg, label %"6", label %"5"

"5":                                              ; preds = %"4"
  unreachable

"6":                                              ; preds = %"4"
  br i1 %arg, label %"10", label %"13"

"10":                                             ; preds = %"6"
  br i1 %arg, label %"22", label %"15"

"13":                                             ; preds = %"6"
  unreachable

"15":                                             ; preds = %"23", %"10"
  unreachable

"22":                                             ; preds = %"10"
  br label %"23"

"23":                                             ; preds = %"1375", %"22"
  %0 = phi i32 [ undef, %"22" ], [ %1, %"1375" ]  ; <i32> [#uses=1]
  indirectswitch ptr undef, [ ptr edgeaddress(@test0, 0), label %"15" ptr edgeaddress(@test0, 1), label %"24" ptr edgeaddress(@test0, 2), label %"25" ptr edgeaddress(@test0, 3), label %"26" ptr edgeaddress(@test0, 4), label %"27" ptr edgeaddress(@test0, 5), label %"28" ptr edgeaddress(@test0, 6), label %"29" ptr edgeaddress(@test0, 7), label %"30" ptr edgeaddress(@test0, 8), label %"32" ptr edgeaddress(@test0, 9), label %"32" ptr edgeaddress(@test0, 10), label %"33" ptr edgeaddress(@test0, 11), label %"167" ptr edgeaddress(@test0, 12), label %"173" ptr edgeaddress(@test0, 13), label %"173" ptr edgeaddress(@test0, 14), label %"173" ptr edgeaddress(@test0, 15), label %"173" ptr edgeaddress(@test0, 16), label %"173" ptr edgeaddress(@test0, 17), label %"192" ptr edgeaddress(@test0, 18), label %"193" ptr edgeaddress(@test0, 19), label %"194" ptr edgeaddress(@test0, 20), label %"196" ptr edgeaddress(@test0, 21), label %"206" ptr edgeaddress(@test0, 22), label %"231" ptr edgeaddress(@test0, 23), label %"241" ptr edgeaddress(@test0, 24), label %"251" ptr edgeaddress(@test0, 25), label %"261" ptr edgeaddress(@test0, 26), label %"307" ptr edgeaddress(@test0, 27), label %"353" ptr edgeaddress(@test0, 28), label %"354" ptr edgeaddress(@test0, 29), label %"355" ptr edgeaddress(@test0, 30), label %"361" ptr edgeaddress(@test0, 31), label %"367" ptr edgeaddress(@test0, 32), label %"400" ptr edgeaddress(@test0, 33), label %"433" ptr edgeaddress(@test0, 34), label %"466" ptr edgeaddress(@test0, 35), label %"499" ptr edgeaddress(@test0, 36), label %"509" ptr edgeaddress(@test0, 37), label %"519" ptr edgeaddress(@test0, 38), label %"529" ptr edgeaddress(@test0, 39), label %"571" ptr edgeaddress(@test0, 40), label %"589" ptr edgeaddress(@test0, 41), label %"607" ptr edgeaddress(@test0, 42), label %"635" ptr edgeaddress(@test0, 43), label %"655" ptr edgeaddress(@test0, 44), label %"664" ptr edgeaddress(@test0, 45), label %"671" ptr edgeaddress(@test0, 46), label %"680" ptr edgeaddress(@test0, 47), label %"687" ptr edgeaddress(@test0, 48), label %"692" ptr edgeaddress(@test0, 49), label %"698" ptr edgeaddress(@test0, 50), label %"704" ptr edgeaddress(@test0, 51), label %"715" ptr edgeaddress(@test0, 52), label %"715" ptr edgeaddress(@test0, 53), label %"716" ptr edgeaddress(@test0, 54), label %"725" ptr edgeaddress(@test0, 55), label %"725" ptr edgeaddress(@test0, 56), label %"725" ptr edgeaddress(@test0, 57), label %"725" ptr edgeaddress(@test0, 58), label %"724" ptr edgeaddress(@test0, 59), label %"724" ptr edgeaddress(@test0, 60), label %"724" ptr edgeaddress(@test0, 61), label %"724" ptr edgeaddress(@test0, 62), label %"737" ptr edgeaddress(@test0, 63), label %"737" ptr edgeaddress(@test0, 64), label %"737" ptr edgeaddress(@test0, 65), label %"737" ptr edgeaddress(@test0, 66), label %"761" ptr edgeaddress(@test0, 67), label %"758" ptr edgeaddress(@test0, 68), label %"759" ptr edgeaddress(@test0, 69), label %"760" ptr edgeaddress(@test0, 70), label %"766" ptr edgeaddress(@test0, 71), label %"763" ptr edgeaddress(@test0, 72), label %"764" ptr edgeaddress(@test0, 73), label %"765" ptr edgeaddress(@test0, 74), label %"771" ptr edgeaddress(@test0, 75), label %"768" ptr edgeaddress(@test0, 76), label %"769" ptr edgeaddress(@test0, 77), label %"770" ptr edgeaddress(@test0, 78), label %"780" ptr edgeaddress(@test0, 79), label %"777" ptr edgeaddress(@test0, 80), label %"778" ptr edgeaddress(@test0, 81), label %"779" ptr edgeaddress(@test0, 82), label %"821" ptr edgeaddress(@test0, 83), label %"826" ptr edgeaddress(@test0, 84), label %"831" ptr edgeaddress(@test0, 85), label %"832" ptr edgeaddress(@test0, 86), label %"833" ptr edgeaddress(@test0, 87), label %"836" ptr edgeaddress(@test0, 88), label %"836" ptr edgeaddress(@test0, 89), label %"886" ptr edgeaddress(@test0, 90), label %"905" ptr edgeaddress(@test0, 91), label %"978" ptr edgeaddress(@test0, 92), label %"978" ptr edgeaddress(@test0, 93), label %"1136" ptr edgeaddress(@test0, 94), label %"1166" ptr edgeaddress(@test0, 95), label %"1179" ptr edgeaddress(@test0, 96), label %"1201" ptr edgeaddress(@test0, 97), label %"1212" ptr edgeaddress(@test0, 98), label %"1212" ptr edgeaddress(@test0, 99), label %"1274" ptr edgeaddress(@test0, 100), label %"1284" ptr edgeaddress(@test0, 101), label %"1284" ptr edgeaddress(@test0, 102), label %"1346" ptr edgeaddress(@test0, 103), label %"1347" ptr edgeaddress(@test0, 104), label %"1348" ptr edgeaddress(@test0, 105), label %"1349" ptr edgeaddress(@test0, 106), label %"1350" ptr edgeaddress(@test0, 107), label %"1353" ptr edgeaddress(@test0, 108), label %"1353" ptr edgeaddress(@test0, 109), label %"1353" ptr edgeaddress(@test0, 110), label %"1355" ptr edgeaddress(@test0, 111), label %"1355" ptr edgeaddress(@test0, 112), label %"1357" ptr edgeaddress(@test0, 113), label %"1357" ptr edgeaddress(@test0, 114), label %"1358" ptr edgeaddress(@test0, 115), label %"1359" ptr edgeaddress(@test0, 116), label %"1374" ptr edgeaddress(@test0, 117), label %"1375" ptr edgeaddress(@test0, 118), label %"1376" ptr edgeaddress(@test0, 119), label %"1377" ptr edgeaddress(@test0, 120), label %"1378" ptr edgeaddress(@test0, 121), label %"1379" ptr edgeaddress(@test0, 122), label %"1386" ptr edgeaddress(@test0, 123), label %"1395" ptr edgeaddress(@test0, 124), label %"1394" ptr edgeaddress(@test0, 125), label %"1425" ptr edgeaddress(@test0, 126), label %"1426" ptr edgeaddress(@test0, 127), label %"1440" ptr edgeaddress(@test0, 128), label %"1449" ptr edgeaddress(@test0, 129), label %"1455" ptr edgeaddress(@test0, 130), label %"1461" ptr edgeaddress(@test0, 131), label %"1471" ptr edgeaddress(@test0, 132), label %"1482" ptr edgeaddress(@test0, 133), label %"1484" ptr edgeaddress(@test0, 134), label %"1486" ptr edgeaddress(@test0, 135), label %"1489" ptr edgeaddress(@test0, 136), label %"1489" ptr edgeaddress(@test0, 137), label %"1492" ptr edgeaddress(@test0, 138), label %"1494" ptr edgeaddress(@test0, 139), label %"1494" ptr edgeaddress(@test0, 140), label %"1497" ptr edgeaddress(@test0, 141), label %"1499" ptr edgeaddress(@test0, 142), label %"1499" ptr edgeaddress(@test0, 143), label %"1515" ptr edgeaddress(@test0, 144), label %"1546" ptr edgeaddress(@test0, 145), label %"1546" ptr edgeaddress(@test0, 146), label %"1566" ptr edgeaddress(@test0, 147), label %"1584" ptr edgeaddress(@test0, 148), label %"1587" ptr edgeaddress(@test0, 149), label %"1591" ptr edgeaddress(@test0, 150), label %"1605" ptr edgeaddress(@test0, 151), label %"1609" ptr edgeaddress(@test0, 152), label %"1609" ptr edgeaddress(@test0, 153), label %"1640" ptr edgeaddress(@test0, 154), label %"1648" ptr edgeaddress(@test0, 155), label %"1651" ptr edgeaddress(@test0, 156), label %"1703" ptr edgeaddress(@test0, 157), label %"1710" ptr edgeaddress(@test0, 158), label %"1718" ptr edgeaddress(@test0, 159), label %"1724" ptr edgeaddress(@test0, 160), label %"1725" ptr edgeaddress(@test0, 161), label %"1726" ptr edgeaddress(@test0, 162), label %"1727" ptr edgeaddress(@test0, 163), label %"1728" ptr edgeaddress(@test0, 164), label %"1731" ptr edgeaddress(@test0, 165), label %"1732" ptr edgeaddress(@test0, 166), label %"1733" ptr edgeaddress(@test0, 167), label %"1734" ptr edgeaddress(@test0, 168), label %"1735" ptr edgeaddress(@test0, 169), label %"1741" ptr edgeaddress(@test0, 170), label %"1750" ptr edgeaddress(@test0, 171), label %"1752" ptr edgeaddress(@test0, 172), label %"1754" ptr edgeaddress(@test0, 173), label %"1755" ptr edgeaddress(@test0, 174), label %"1757" ptr edgeaddress(@test0, 175), label %"1759" ptr edgeaddress(@test0, 176), label %"1761" ptr edgeaddress(@test0, 177), label %"1764" ptr edgeaddress(@test0, 178), label %"1764" ptr edgeaddress(@test0, 179), label %"1766" ptr edgeaddress(@test0, 180), label %"1768" ptr edgeaddress(@test0, 181), label %"1775" ptr edgeaddress(@test0, 182), label %"1775" ptr edgeaddress(@test0, 183), label %"1781" ptr edgeaddress(@test0, 184), label %"1781" ptr edgeaddress(@test0, 185), label %"1790" ptr edgeaddress(@test0, 186), label %"1827" ptr edgeaddress(@test0, 187), label %"1836" ptr edgeaddress(@test0, 188), label %"1836" ptr edgeaddress(@test0, 189), label %"1845" ptr edgeaddress(@test0, 190), label %"1845" ptr edgeaddress(@test0, 191), label %"1848" ptr edgeaddress(@test0, 192), label %"1849" ptr edgeaddress(@test0, 193), label %"1851" ptr edgeaddress(@test0, 194), label %"1853" ptr edgeaddress(@test0, 195), label %"1856" ptr edgeaddress(@test0, 196), label %"1861" ptr edgeaddress(@test0, 197), label %"1861" ]

"24":                                             ; preds = %"23"
  unreachable

"25":                                             ; preds = %"23"
  unreachable

"26":                                             ; preds = %"23"
  unreachable

"27":                                             ; preds = %"23"
  unreachable

"28":                                             ; preds = %"23"
  unreachable

"29":                                             ; preds = %"23"
  unreachable

"30":                                             ; preds = %"23"
  unreachable

"32":                                             ; preds = %"23", %"23"
  unreachable

"33":                                             ; preds = %"23"
  unreachable

"167":                                            ; preds = %"23"
  unreachable

"173":                                            ; preds = %"23", %"23", %"23", %"23", %"23"
  unreachable

"192":                                            ; preds = %"23"
  unreachable

"193":                                            ; preds = %"23"
  unreachable

"194":                                            ; preds = %"23"
  unreachable

"196":                                            ; preds = %"23"
  unreachable

"206":                                            ; preds = %"23"
  unreachable

"231":                                            ; preds = %"23"
  unreachable

"241":                                            ; preds = %"23"
  unreachable

"251":                                            ; preds = %"23"
  unreachable

"261":                                            ; preds = %"23"
  unreachable

"307":                                            ; preds = %"23"
  unreachable

"353":                                            ; preds = %"23"
  unreachable

"354":                                            ; preds = %"23"
  unreachable

"355":                                            ; preds = %"23"
  unreachable

"361":                                            ; preds = %"23"
  unreachable

"367":                                            ; preds = %"23"
  unreachable

"400":                                            ; preds = %"23"
  unreachable

"433":                                            ; preds = %"23"
  unreachable

"466":                                            ; preds = %"23"
  unreachable

"499":                                            ; preds = %"23"
  unreachable

"509":                                            ; preds = %"23"
  unreachable

"519":                                            ; preds = %"23"
  unreachable

"529":                                            ; preds = %"23"
  unreachable

"571":                                            ; preds = %"23"
  unreachable

"589":                                            ; preds = %"23"
  unreachable

"607":                                            ; preds = %"23"
  unreachable

"635":                                            ; preds = %"23"
  unreachable

"655":                                            ; preds = %"23"
  unreachable

"664":                                            ; preds = %"23"
  unreachable

"671":                                            ; preds = %"23"
  unreachable

"680":                                            ; preds = %"23"
  unreachable

"687":                                            ; preds = %"23"
  unreachable

"692":                                            ; preds = %"23"
  br label %"1862"

"698":                                            ; preds = %"23"
  unreachable

"704":                                            ; preds = %"23"
  unreachable

"715":                                            ; preds = %"23", %"23"
  unreachable

"716":                                            ; preds = %"23"
  unreachable

"724":                                            ; preds = %"23", %"23", %"23", %"23"
  unreachable

"725":                                            ; preds = %"23", %"23", %"23", %"23"
  unreachable

"737":                                            ; preds = %"23", %"23", %"23", %"23"
  unreachable

"758":                                            ; preds = %"23"
  unreachable

"759":                                            ; preds = %"23"
  unreachable

"760":                                            ; preds = %"23"
  unreachable

"761":                                            ; preds = %"23"
  unreachable

"763":                                            ; preds = %"23"
  unreachable

"764":                                            ; preds = %"23"
  unreachable

"765":                                            ; preds = %"23"
  br label %"766"

"766":                                            ; preds = %"765", %"23"
  unreachable

"768":                                            ; preds = %"23"
  unreachable

"769":                                            ; preds = %"23"
  unreachable

"770":                                            ; preds = %"23"
  unreachable

"771":                                            ; preds = %"23"
  unreachable

"777":                                            ; preds = %"23"
  unreachable

"778":                                            ; preds = %"23"
  unreachable

"779":                                            ; preds = %"23"
  unreachable

"780":                                            ; preds = %"23"
  unreachable

"821":                                            ; preds = %"23"
  unreachable

"826":                                            ; preds = %"23"
  unreachable

"831":                                            ; preds = %"23"
  unreachable

"832":                                            ; preds = %"23"
  unreachable

"833":                                            ; preds = %"23"
  unreachable

"836":                                            ; preds = %"23", %"23"
  unreachable

"886":                                            ; preds = %"23"
  unreachable

"905":                                            ; preds = %"23"
  unreachable

"978":                                            ; preds = %"23", %"23"
  unreachable

"1136":                                           ; preds = %"23"
  unreachable

"1166":                                           ; preds = %"23"
  unreachable

"1179":                                           ; preds = %"23"
  unreachable

"1201":                                           ; preds = %"23"
  unreachable

"1212":                                           ; preds = %"23", %"23"
  unreachable

"1274":                                           ; preds = %"23"
  unreachable

"1284":                                           ; preds = %"23", %"23"
  unreachable

"1346":                                           ; preds = %"23"
  unreachable

"1347":                                           ; preds = %"23"
  unreachable

"1348":                                           ; preds = %"23"
  unreachable

"1349":                                           ; preds = %"23"
  unreachable

"1350":                                           ; preds = %"23"
  unreachable

"1353":                                           ; preds = %"23", %"23", %"23"
  unreachable

"1355":                                           ; preds = %"23", %"23"
  unreachable

"1357":                                           ; preds = %"23", %"23"
  unreachable

"1358":                                           ; preds = %"23"
  unreachable

"1359":                                           ; preds = %"23"
  unreachable

"1374":                                           ; preds = %"23"
  unreachable

"1375":                                           ; preds = %"23"
  %1 = zext i8 undef to i32                       ; <i32> [#uses=1]
  br label %"23"

"1376":                                           ; preds = %"23"
  unreachable

"1377":                                           ; preds = %"23"
  unreachable

"1378":                                           ; preds = %"23"
  unreachable

"1379":                                           ; preds = %"23"
  unreachable

"1386":                                           ; preds = %"23"
  unreachable

"1394":                                           ; preds = %"23"
  unreachable

"1395":                                           ; preds = %"23"
  unreachable

"1425":                                           ; preds = %"23"
  unreachable

"1426":                                           ; preds = %"23"
  unreachable

"1440":                                           ; preds = %"23"
  unreachable

"1449":                                           ; preds = %"23"
  unreachable

"1455":                                           ; preds = %"23"
  unreachable

"1461":                                           ; preds = %"23"
  unreachable

"1471":                                           ; preds = %"23"
  unreachable

"1482":                                           ; preds = %"23"
  unreachable

"1484":                                           ; preds = %"23"
  unreachable

"1486":                                           ; preds = %"23"
  unreachable

"1489":                                           ; preds = %"23", %"23"
  unreachable

"1492":                                           ; preds = %"23"
  unreachable

"1494":                                           ; preds = %"23", %"23"
  unreachable

"1497":                                           ; preds = %"23"
  unreachable

"1499":                                           ; preds = %"23", %"23"
  unreachable

"1515":                                           ; preds = %"23"
  unreachable

"1546":                                           ; preds = %"23", %"23"
  unreachable

"1566":                                           ; preds = %"23"
  br i1 %arg, label %"1569", label %"1568"

"1568":                                           ; preds = %"1566"
  unreachable

"1569":                                           ; preds = %"1566"
  unreachable

"1584":                                           ; preds = %"23"
  unreachable

"1587":                                           ; preds = %"23"
  unreachable

"1591":                                           ; preds = %"23"
  unreachable

"1605":                                           ; preds = %"23"
  unreachable

"1609":                                           ; preds = %"23", %"23"
  unreachable

"1640":                                           ; preds = %"23"
  unreachable

"1648":                                           ; preds = %"23"
  unreachable

"1651":                                           ; preds = %"23"
  unreachable

"1703":                                           ; preds = %"23"
  unreachable

"1710":                                           ; preds = %"23"
  unreachable

"1718":                                           ; preds = %"23"
  unreachable

"1724":                                           ; preds = %"23"
  unreachable

"1725":                                           ; preds = %"23"
  unreachable

"1726":                                           ; preds = %"23"
  unreachable

"1727":                                           ; preds = %"23"
  unreachable

"1728":                                           ; preds = %"23"
  unreachable

"1731":                                           ; preds = %"23"
  unreachable

"1732":                                           ; preds = %"23"
  unreachable

"1733":                                           ; preds = %"23"
  unreachable

"1734":                                           ; preds = %"23"
  unreachable

"1735":                                           ; preds = %"23"
  unreachable

"1741":                                           ; preds = %"23"
  unreachable

"1750":                                           ; preds = %"23"
  unreachable

"1752":                                           ; preds = %"23"
  unreachable

"1754":                                           ; preds = %"23"
  unreachable

"1755":                                           ; preds = %"23"
  unreachable

"1757":                                           ; preds = %"23"
  unreachable

"1759":                                           ; preds = %"23"
  unreachable

"1761":                                           ; preds = %"23"
  unreachable

"1764":                                           ; preds = %"23", %"23"
  %2 = icmp eq i32 %0, 168                        ; <i1> [#uses=0]
  unreachable

"1766":                                           ; preds = %"23"
  unreachable

"1768":                                           ; preds = %"23"
  unreachable

"1775":                                           ; preds = %"23", %"23"
  unreachable

"1781":                                           ; preds = %"23", %"23"
  unreachable

"1790":                                           ; preds = %"23"
  unreachable

"1827":                                           ; preds = %"23"
  unreachable

"1836":                                           ; preds = %"23", %"23"
  br label %"1862"

"1845":                                           ; preds = %"23", %"23"
  unreachable

"1848":                                           ; preds = %"23"
  unreachable

"1849":                                           ; preds = %"23"
  unreachable

"1851":                                           ; preds = %"23"
  unreachable

"1853":                                           ; preds = %"23"
  unreachable

"1856":                                           ; preds = %"23"
  unreachable

"1861":                                           ; preds = %"23", %"23"
  unreachable

"41":                                             ; preds = %"23", %"23"
  unreachable

"1862":                                           ; preds = %"1836", %"692"
  unreachable
}

; An exit for Loop L1 may be the header of a disjoint Loop L2.  Thus, when we
; create PHIs in one of such exits we are also inserting PHIs in L2 header. This
; could break LCSSA form for L2 because these inserted PHIs can also have uses
; in L2 exits. Test that we don't assert/crash on that.
define void @test1(i1 %arg) {
; CHECK-LABEL: @test1
  br label %lab1

lab1:
  %tmp21 = add i32 undef, 677038203
  br i1 %arg, label %lab2, label %exit

lab2:
  indirectswitch ptr undef, [ ptr edgeaddress(@test1, 0), label %lab1 ptr edgeaddress(@test1, 1), label %lab3 ]

lab3:
; CHECK: lab3.preheader:
; CHECK-NEXT: %tmp21.lcssa1 = phi i32 [ %tmp21, %lab2 ]
  %tmp12 = phi i32 [ %tmp21, %lab2 ], [ %tmp12, %lab4 ]
  br i1 %arg, label %lab5, label %lab4

lab4:
  br label %lab3

lab5:
; CHECK: lab5:
; CHECK-NEXT: %tmp15 = add i32 %tmp21.lcssa1, undef
  %tmp15 = add i32 %tmp12, undef
  br label %exit

exit:
  ret void
}
