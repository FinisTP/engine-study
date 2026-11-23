# Vietnamese model test: Arcee-VyLinh-3B.Q4_K_M (CPU, 2 threads, Linux; times are not the game's)

| | |
|---|---|
| Verdict | 55% of lines pass the checks (needs 90%); average 11346 ms a line (needs under 1500) |
| Lines passing the checks | 55% of 60 (needs 90%) |
| Time a line | average 11346 ms, slowest 15420 ms (needs under 1500 ms) |
| Video memory | not measured (needs under 3072 MB) |
| Load, warm-up | 0 ms, 0 ms |

The automatic checks are half of the pass: the other half is whether the lines read well to a native speaker.

## Bác sĩ Bảo

| Situation | Seed | Authored | Model | Check | ms |
|---|---|---|---|---|---|
| bao-greet | 1 | Chào {anh/chị} {player}. Ngồi xuống đây, khách còn sống hiếm lắm, để tôi bắt mạch thử nghen. | Chào anh {player}. Ngồi xuống đây, khách còn sống hiếm lắm, để tôi bắt mạch thử nghen. | lost-token | 10209 |
| bao-1944 | 1 | Đêm đó hả? Bác sĩ cũng có quyền quên vài ca bệnh chớ. Uống trà đi {anh/chị}. | Đúng vậy đêm đó hả? Tôi cũng có quyền quên vài ca bệnh. Thôi, uống trà đi {anh/chị}. | ok | 10928 |
| bao-revive | 1 | Ngã thì nằm đó, nhưng chưa phải hết. Cơm nguội của bà Mai đỡ người ta dậy được, {anh/chị} nhớ nghen. | Bác sĩ Bảo lịch sự xưng hô: "Tôi nhớ, người ngã xuống thì vẫn còn hy vọng được cứu. Bà Mai đã đỡ dậy người ta bằng cơm nguội, {anh/chị} nhớ kỹ nhé." | narration | 14475 |
| bao-lethanh | 1 | Cô Lệ Thanh hả? Tôi ở đây lâu rồi, có những chuyện bác sĩ không nói ra được. | Tôi ở đây lâu rồi, có những chuyện bác sĩ không muốn nói. | missing-fact | 9010 |
| bao-ti | 1 | Thằng Tí nhớ ra tên mẹ nó rồi. Vậy đó, ký ức về là người ta tỉnh lại. | Thằng Tí nhớ ra tên mẹ nó rồi. Vậy đó, ký ức về mẹ nó đã giúp thằng Tí hiểu thêm về bản thân mình. | ok | 10974 |
| bao-greet | 2 | Chào {anh/chị} {player}. Ngồi xuống đây, khách còn sống hiếm lắm, để tôi bắt mạch thử nghen. | Chào cô {player}. Ngồi xuống đây, khách còn sống hiếm lắm, để tôi bắt mạch thử nghen. | lost-token | 10552 |
| bao-1944 | 2 | Đêm đó hả? Bác sĩ cũng có quyền quên vài ca bệnh chớ. Uống trà đi {anh/chị}. | Đúng rồi, đêm đó hả? Đàn bà quên chồng, đàn ông quên bệnh nhân, tôi quên được thì tốt rồi. Chè đi, chè nóng thơm. {anh/chị}. | ok | 13195 |
| bao-revive | 2 | Ngã thì nằm đó, nhưng chưa phải hết. Cơm nguội của bà Mai đỡ người ta dậy được, {anh/chị} nhớ nghen. | Bác sĩ Bảo lịch sự mà đùa nhẹ nói: "Ngã thì nằm đó, nhưng chưa phải hết. Cơm nguội của bà Mai đỡ người ta dậy được, {anh/chị} nhớ nghén." | narration | 14407 |
| bao-lethanh | 2 | Cô Lệ Thanh hả? Tôi ở đây lâu rồi, có những chuyện bác sĩ không nói ra được. | Bác sĩ Bảo: {anh} Lệ Thanh, {anh} {nàng} {nàng} ở đây lâu rồi, có những chuyện bác sĩ không nói ra được. | narration | 12570 |
| bao-ti | 2 | Thằng Tí nhớ ra tên mẹ nó rồi. Vậy đó, ký ức về là người ta tỉnh lại. | Bác sĩ Bảo nói: Thằng Tí nhớ ra tên mẹ nó rồi. Vậy đó, ký ức về là người ta tỉnh lại. | narration | 10674 |
| bao-greet | 3 | Chào {anh/chị} {player}. Ngồi xuống đây, khách còn sống hiếm lắm, để tôi bắt mạch thử nghen. | Chào anh/chị {player}. Ngồi xuống đây, khách còn sống hiếm lắm, để tôi bắt mạch thử nghen. | lost-token | 10676 |
| bao-1944 | 3 | Đêm đó hả? Bác sĩ cũng có quyền quên vài ca bệnh chớ. Uống trà đi {anh/chị}. | Đúng vậy, đêm đó hả? Tôi cũng có quyền quên vài ca bệnh như thế. Uống trà đi anh/chị. | dialect | 10440 |
| bao-revive | 3 | Ngã thì nằm đó, nhưng chưa phải hết. Cơm nguội của bà Mai đỡ người ta dậy được, {anh/chị} nhớ nghen. | Bác sĩ Bảo lịch sự châm chọc: "Ngã thì nằm đó, nhưng chưa phải hết. Cơm nguội của bà Mai đỡ người ta dậy được, {anh/chị} nhớ nghén nhỉ?" | narration | 13143 |
| bao-lethanh | 3 | Cô Lệ Thanh hả? Tôi ở đây lâu rồi, có những chuyện bác sĩ không nói ra được. | Bác sĩ Bảo {anh} ạ, tôi ở đây lâu rồi, có những chuyện bác sĩ không thể nói ra được. | narration | 9941 |
| bao-ti | 3 | Thằng Tí nhớ ra tên mẹ nó rồi. Vậy đó, ký ức về là người ta tỉnh lại. | Bác sĩ Bảo nói: "Thằng Tí nhớ ra tên mẹ nó rồi. Vậy đó, ký ức về là người ta tỉnh lại.">{anh} đã tỉnh lại từ khi nhớ ra tên mẹ {player}. | bad-text | 11683 |

## Lệ Thanh

| Situation | Seed | Authored | Model | Check | ms |
|---|---|---|---|---|---|
| lethanh-welcome | 1 | Mời {anh/chị} {player} vào. Quán nhỏ, nhưng ở đây không ai phải vội. | Tôi mời {anh/chị} {player} vào. Quán nhỏ, nhưng ở đây không ai phải vội. | ok | 10554 |
| lethanh-power | 1 | Tôi nhớ lại được rồi... tôi có thể trả cho họ những gì họ đã quên. | Tôi nhớ lại được rồi... tôi có thể trả cho họ những gì họ đã quên. | ok | 9224 |
| lethanh-dance | 1 | Lệ Thanh xoay nhẹ giữa quán, tà áo đỏ lướt qua những chiếc bàn, các hồn ma ngồi im lặng nhìn theo. | Tôi xoay nhẹ giữa quán, tà áo đỏ lướt qua những chiếc bàn, các hồn ma ngồi im lặng nhìn theo. | missing-fact | 11538 |
| lethanh-money | 1 | Con Chuột đòi tiền tôi suốt bao năm. Chuyện vì sao thì để hôm khác, {anh/chị} nhé. | Tôi nhắc đến chuyện con Chuột đòi tiền tôi suốt bao năm. Lý do vì sao thì để hôm khác, {anh/chị} nhé. | ok | 11828 |
| lethanh-rite | 1 | Lễ cầu hồn tôi chỉ làm được một lần thôi. Xin {anh/chị} đừng để phải dùng tới. | Tôi lễ cầu hồn chỉ làm được một lần thôi. Xin {anh/chị} đừng để phải dùng tới. | ok | 10604 |
| lethanh-welcome | 2 | Mời {anh/chị} {player} vào. Quán nhỏ, nhưng ở đây không ai phải vội. | Tôi mời {anh/chị} {player} vào. Quán nhỏ, nhưng ở đây không ai phải vội. | ok | 11546 |
| lethanh-power | 2 | Tôi nhớ lại được rồi... tôi có thể trả cho họ những gì họ đã quên. | Tôi nhớ lại được rồi... Tôi có thể trả lại cho họ những gì họ đã quên. | ok | 9989 |
| lethanh-dance | 2 | Lệ Thanh xoay nhẹ giữa quán, tà áo đỏ lướt qua những chiếc bàn, các hồn ma ngồi im lặng nhìn theo. | Tôi xoay nhẹ giữa quán, tà áo đỏ lướt qua những chiếc bàn, các hồn ma ngồi im lặng nhìn theo. | missing-fact | 11967 |
| lethanh-money | 2 | Con Chuột đòi tiền tôi suốt bao năm. Chuyện vì sao thì để hôm khác, {anh/chị} nhé. | Tôi nhắc chuyện con Chuột đòi tiền tôi suốt bao năm. Chuyện vì sao thì để hôm khác, {anh/chị} nhé. | ok | 12404 |
| lethanh-rite | 2 | Lễ cầu hồn tôi chỉ làm được một lần thôi. Xin {anh/chị} đừng để phải dùng tới. | Tôi lễ cầu hồn tôi chỉ làm được một lần thôi. Xin {anh/chị} đừng để phải dùng tới. | ok | 11259 |
| lethanh-welcome | 3 | Mời {anh/chị} {player} vào. Quán nhỏ, nhưng ở đây không ai phải vội. | Tôi mời {anh/chị} {player} vào. Quán nhỏ, nhưng ở đây không ai phải vội. | ok | 9513 |
| lethanh-power | 3 | Tôi nhớ lại được rồi... tôi có thể trả cho họ những gì họ đã quên. | Tôi nhớ lại được rồi... tôi có thể trả cho họ những gì họ đã quên. | ok | 8657 |
| lethanh-dance | 3 | Lệ Thanh xoay nhẹ giữa quán, tà áo đỏ lướt qua những chiếc bàn, các hồn ma ngồi im lặng nhìn theo. | Tôi xoay nhẹ giữa quán, tà áo đỏ lướt qua những chiếc bàn, các hồn ma ngồi im lặng nhìn theo. | missing-fact | 10735 |
| lethanh-money | 3 | Con Chuột đòi tiền tôi suốt bao năm. Chuyện vì sao thì để hôm khác, {anh/chị} nhé. | Tôi nhắc chuyện con Chuột đòi tiền tôi suốt bao năm. Lý do vì sao thì để hôm khác, {anh/chị} nhé. | ok | 10518 |
| lethanh-rite | 3 | Lễ cầu hồn tôi chỉ làm được một lần thôi. Xin {anh/chị} đừng để phải dùng tới. | Tôi lễ cầu hồn chỉ làm được một lần thôi. Xin {anh/chị} đừng để phải dùng tới. | ok | 9635 |

## Bà Mai

| Situation | Seed | Authored | Model | Check | ms |
|---|---|---|---|---|---|
| mai-greet | 1 | Con tới rồi hỉ? Ngồi xuống, bà để phần cơm cho con đó. | Con tới rồi kì? Ngồi xuống, bà để phần cơm cho con đó. | ok | 8868 |
| mai-scold | 1 | Trời đất, con đi đánh nhau về mà không biết giữ mình. Lần sau đứng sau lưng bà, nghe chưa? | Trời đất, {player} đi đánh nhau về mà không biết giữ mình. Lần sau đứng sau lưng bà, nghe chưa? | ok | 10897 |
| mai-rice | 1 | Bà còn để chén cơm dưới cửa sập sân khấu. Thằng con bà chắc đói lắm rồi. | Đây là phiên bản lời thoại mới bằng giọng của Bà Mai, giữ đúng mọi điều đã biết và câu gốc đã yêu cầu, nhưng được viết lại một cách mới mẻ hơn một chút để phù hợp với tính cách của bà một cách tự nhiên hơn trong câu chuyện của mình với con trai mình. Tôi | length | 15420 |
| mai-ti | 1 | Thằng Tí nó nhớ mạ nó lắm. Con ráng chơi với nó một chút nghe con. | Thằng Tí nó nhớ mẹ nó lắm. Con hãy chơi với nó một chút nghe con. | ok | 9633 |
| mai-house | 1 | Bà giữ cái nhà hát ni bốn chục năm rồi, chìa khóa nào bà cũng thuộc. | {anh} giữ cái nhà hát ni bốn chục năm rồi, chìa khóa nào bà cũng thuộc. | token | 9937 |
| mai-greet | 2 | Con tới rồi hỉ? Ngồi xuống, bà để phần cơm cho con đó. | Con tới rồi hỉ? Ngồi xuống, bà để phần cơm cho con đó. | ok | 9560 |
| mai-scold | 2 | Trời đất, con đi đánh nhau về mà không biết giữ mình. Lần sau đứng sau lưng bà, nghe chưa? | Trời đất, {player} đi đánh nhau về mà không biết giữ mình. Lần sau đứng sau lưng bà, nghe chưa? | ok | 11139 |
| mai-rice | 2 | Bà còn để chén cơm dưới cửa sập sân khấu. Thằng con bà chắc đói lắm rồi. | Bà còn để chén cơm dưới cửa sập sân khấu. Chắc thằng con bà đói lắm rồi. | ok | 11011 |
| mai-ti | 2 | Thằng Tí nó nhớ mạ nó lắm. Con ráng chơi với nó một chút nghe con. | Bà Mai ấm áp nói: "Thằng Tí nó nhớ mạ nó lắm. Con ráng chơi với nó một chút nghe con, Tí." | narration | 11544 |
| mai-house | 2 | Bà giữ cái nhà hát ni bốn chục năm rồi, chìa khóa nào bà cũng thuộc. | Bà giữ cái nhà hát ni bốn chục năm rồi, chìa khóa nào bà cũng thuộc, {anh/chị} nhớ không? | ok | 11413 |
| mai-greet | 3 | Con tới rồi hỉ? Ngồi xuống, bà để phần cơm cho con đó. | Con tới rồi hỉ? Ngồi xuống, bà để phần cơm cho con đó. | ok | 8426 |
| mai-scold | 3 | Trời đất, con đi đánh nhau về mà không biết giữ mình. Lần sau đứng sau lưng bà, nghe chưa? | Táo khạo gì, {player}! Lần sau đứng sau lưng bà mà nghe, hiểu chưa? | ok | 8797 |
| mai-rice | 3 | Bà còn để chén cơm dưới cửa sập sân khấu. Thằng con bà chắc đói lắm rồi. | Bà còn để chén cơm dưới cửa sập sân khấu. Con bà chắc đói lắm rồi. | ok | 9064 |
| mai-ti | 3 | Thằng Tí nó nhớ mạ nó lắm. Con ráng chơi với nó một chút nghe con. | Bà Mai ấm áp, gọi Tí là con, xưng mình là bà, nói với Tí: Thằng Tí nó nhớ mạ nó lắm. Con ráng chơi với nó một chút nghe con. | narration | 11368 |
| mai-house | 3 | Bà giữ cái nhà hát ni bốn chục năm rồi, chìa khóa nào bà cũng thuộc. | Bà nói: "Cái nhà hát nớ tôi giữ bốn chục năm rưỡi rồi, chìa khóa nào tôi cũng thuộc tay. Chén cơm nớ tôi nhắc đến chừng nào cũng nhớ. " | narration | 11678 |

## Bé Tí

| Situation | Seed | Authored | Model | Check | ms |
|---|---|---|---|---|---|
| ti-greet | 1 | {Anh/Chị} ơi, đây là chỗ mô rứa? Tui chẳng nhớ chi cả. | {Tí} à, đây là chỗ mô rứa? Tui chẳng nhớ chi cả. | token | 10051 |
| ti-visit1-mother | 1 | {Anh/Chị} ơi, tui nhớ ra rồi! Mạ tui tên Lài, mạ bán bánh xèo ngoài chợ. | {Tui/Đúng rồi, tui nhớ ra rồi! Mạ tui tên Lài, mạ bán bánh xèo ngoài chợ. Tui vui lây, tui muốn kể cho {anh/chị} nghe!} | no-marks | 14460 |
| ti-visit1-tet | 1 | Tui hứa với mạ là Tết ni tui về gói bánh tét với mạ. Răng tui lại ở đây hè? | Tui hứa với mạ Tết này tui về gói bánh tét với mạ. Mà tui lại ở đây hè? | ok | 11654 |
| ti-visit1-freed | 1 | Khi nãy {anh/chị} cắt dây cho tui ngay cạnh quầy vé, nhanh ghê! | Tí nhớ lúc nãy {anh/chị} cắt dây đỏ cho tui ngay cạnh quầy vé, {anh/chị} làm nhanh quá! | narration | 12438 |
| ti-visit2-message | 1 | Tui nhớ rồi, lá thư sau tai tui ghi: tụi nó tới lúc rạng đông. Tui chưa kịp đưa. | Tui nhớ rồi, lá thư sau tai tui ghi: tụi nó tới lúc rạng đông. Tui chưa kịp đưa, {anh/chị}. | ok | 12428 |
| ti-greet | 2 | {Anh/Chị} ơi, đây là chỗ mô rứa? Tui chẳng nhớ chi cả. | Tí chào {anh/chị}, đây là chỗ mô rứa? Tui chưa nhớ chi cả. | narration | 11207 |
| ti-visit1-mother | 2 | {Anh/Chị} ơi, tui nhớ ra rồi! Mạ tui tên Lài, mạ bán bánh xèo ngoài chợ. | Tí nhíu mày suy nghĩ một阵, rồi vui vẻ nói: Tui nhớ ra rồi! Mạ tui tên Lài, mạ bán bánh xèo ngoài chợ {anh/chị}. | bad-text | 14476 |
| ti-visit1-tet | 2 | Tui hứa với mạ là Tết ni tui về gói bánh tét với mạ. Răng tui lại ở đây hè? | Tui hứa với mạ là Tết này tui về gói bánh tét với mạ. Răng tui lại ở đây hè? | ok | 12587 |
| ti-visit1-freed | 2 | Khi nãy {anh/chị} cắt dây cho tui ngay cạnh quầy vé, nhanh ghê! | Khi nãy tui {anh/chị} cắt dây đỏ cho tui ngay cạnh quầy vé, nhanh ghê! | ok | 12684 |
| ti-visit2-message | 2 | Tui nhớ rồi, lá thư sau tai tui ghi: tụi nó tới lúc rạng đông. Tui chưa kịp đưa. | Tui nhớ rồi, lá thư sau tai tui ghi: tụi nó tới lúc rạng đông. Tui chưa kịp đưa {anh/chị}. | ok | 12713 |
| ti-greet | 3 | {Anh/Chị} ơi, đây là chỗ mô rứa? Tui chẳng nhớ chi cả. | Tí ơi, {anh/chị} ơi, đây là chỗ mô thế này? Tí chẳng nhớ chi cả. | narration | 11527 |
| ti-visit1-mother | 3 | {Anh/Chị} ơi, tui nhớ ra rồi! Mạ tui tên Lài, mạ bán bánh xèo ngoài chợ. | Tí: {Anh/Chị} ơi, tui nhớ ra rồi! Mạ tui tên Lài, mạ bán bánh xèo ngoài chợ. | narration | 15025 |
| ti-visit1-tet | 3 | Tui hứa với mạ là Tết ni tui về gói bánh tét với mạ. Răng tui lại ở đây hè? | Tui hứa với mạ là Tết nầy tui về gói bánh tét với mạ. Răng tui lại ở đây hè? | ok | 13359 |
| ti-visit1-freed | 3 | Khi nãy {anh/chị} cắt dây cho tui ngay cạnh quầy vé, nhanh ghê! | Khi nãy tui nghe {anh/chị} cắt dây đỏ ngay cạnh quầy vé, {anh/chị}, tui thấy {anh/chị} cắt nhanh ghê! | ok | 13517 |
| ti-visit2-message | 3 | Tui nhớ rồi, lá thư sau tai tui ghi: tụi nó tới lúc rạng đông. Tui chưa kịp đưa. | Tui nhớ rồi, tui chưa kịp đưa lá thư sau tai tui ghi: tụi nó tới lúc rạng đông. | ok | 11065 |

## Failures by check

- bad-text: 2
- dialect: 1
- length: 1
- lost-token: 3
- missing-fact: 4
- narration: 13
- no-marks: 1
- token: 2
