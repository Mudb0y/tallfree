/* One stub per page handler: it records its page number and the message it
   was sent, then jumps on to the handler saved in page_orig, unless the
   speech settings menu keeps the message, when it answers 1, handled. */
#define PAGE_STUB(n) \
    __asm__(".syntax unified\n.thumb\n.text\n" \
            ".globl page_stub_" #n "\n.thumb_func\n" \
            "page_stub_" #n ":\n" \
            "    push  {r0-r5, r12, lr}\n" \
            "    mov   r1, r0\n" \
            "    movs  r0, #" #n "\n" \
            "    bl    page_record\n" \
            "    cmp   r0, #0\n" \
            "    pop   {r0-r5, r12, lr}\n" \
            "    bne   1f\n" \
            "    ldr   r12, =page_orig + " #n " * 4\n" \
            "    ldr   r12, [r12]\n" \
            "    bx    r12\n" \
            "1:  movs  r0, #1\n" \
            "    bx    lr\n" \
            ".ltorg\n")

PAGE_STUB(0);
PAGE_STUB(1);
PAGE_STUB(2);
PAGE_STUB(3);
PAGE_STUB(4);
PAGE_STUB(5);
PAGE_STUB(6);
PAGE_STUB(7);
PAGE_STUB(8);
PAGE_STUB(9);
PAGE_STUB(10);
PAGE_STUB(11);
PAGE_STUB(12);
PAGE_STUB(13);
PAGE_STUB(14);
PAGE_STUB(15);
PAGE_STUB(16);
PAGE_STUB(17);
PAGE_STUB(18);
PAGE_STUB(19);
PAGE_STUB(20);
PAGE_STUB(21);
PAGE_STUB(22);
PAGE_STUB(23);
PAGE_STUB(24);
PAGE_STUB(25);
PAGE_STUB(26);
PAGE_STUB(27);
PAGE_STUB(28);
PAGE_STUB(29);
PAGE_STUB(30);
PAGE_STUB(31);
PAGE_STUB(32);
PAGE_STUB(33);
PAGE_STUB(34);
PAGE_STUB(35);
PAGE_STUB(36);
PAGE_STUB(37);
PAGE_STUB(38);
PAGE_STUB(39);
PAGE_STUB(40);
PAGE_STUB(41);
PAGE_STUB(42);
PAGE_STUB(43);
PAGE_STUB(44);
PAGE_STUB(45);
PAGE_STUB(46);
PAGE_STUB(47);
PAGE_STUB(48);
PAGE_STUB(49);
PAGE_STUB(50);
PAGE_STUB(51);
PAGE_STUB(52);
PAGE_STUB(53);
PAGE_STUB(54);
PAGE_STUB(55);
PAGE_STUB(56);
PAGE_STUB(57);
PAGE_STUB(58);
PAGE_STUB(59);
PAGE_STUB(60);
PAGE_STUB(61);
PAGE_STUB(62);
PAGE_STUB(63);
PAGE_STUB(64);
PAGE_STUB(65);
PAGE_STUB(66);
PAGE_STUB(67);
PAGE_STUB(68);
PAGE_STUB(69);
PAGE_STUB(70);
PAGE_STUB(71);
PAGE_STUB(72);
PAGE_STUB(73);
PAGE_STUB(74);
PAGE_STUB(75);
PAGE_STUB(76);
PAGE_STUB(77);
PAGE_STUB(78);
PAGE_STUB(79);
PAGE_STUB(80);
PAGE_STUB(81);
PAGE_STUB(82);
PAGE_STUB(83);
PAGE_STUB(84);
PAGE_STUB(85);
PAGE_STUB(86);
PAGE_STUB(87);
PAGE_STUB(88);
PAGE_STUB(89);
PAGE_STUB(90);
PAGE_STUB(91);
PAGE_STUB(92);
PAGE_STUB(93);

void page_stub_0(void);
void page_stub_1(void);
void page_stub_2(void);
void page_stub_3(void);
void page_stub_4(void);
void page_stub_5(void);
void page_stub_6(void);
void page_stub_7(void);
void page_stub_8(void);
void page_stub_9(void);
void page_stub_10(void);
void page_stub_11(void);
void page_stub_12(void);
void page_stub_13(void);
void page_stub_14(void);
void page_stub_15(void);
void page_stub_16(void);
void page_stub_17(void);
void page_stub_18(void);
void page_stub_19(void);
void page_stub_20(void);
void page_stub_21(void);
void page_stub_22(void);
void page_stub_23(void);
void page_stub_24(void);
void page_stub_25(void);
void page_stub_26(void);
void page_stub_27(void);
void page_stub_28(void);
void page_stub_29(void);
void page_stub_30(void);
void page_stub_31(void);
void page_stub_32(void);
void page_stub_33(void);
void page_stub_34(void);
void page_stub_35(void);
void page_stub_36(void);
void page_stub_37(void);
void page_stub_38(void);
void page_stub_39(void);
void page_stub_40(void);
void page_stub_41(void);
void page_stub_42(void);
void page_stub_43(void);
void page_stub_44(void);
void page_stub_45(void);
void page_stub_46(void);
void page_stub_47(void);
void page_stub_48(void);
void page_stub_49(void);
void page_stub_50(void);
void page_stub_51(void);
void page_stub_52(void);
void page_stub_53(void);
void page_stub_54(void);
void page_stub_55(void);
void page_stub_56(void);
void page_stub_57(void);
void page_stub_58(void);
void page_stub_59(void);
void page_stub_60(void);
void page_stub_61(void);
void page_stub_62(void);
void page_stub_63(void);
void page_stub_64(void);
void page_stub_65(void);
void page_stub_66(void);
void page_stub_67(void);
void page_stub_68(void);
void page_stub_69(void);
void page_stub_70(void);
void page_stub_71(void);
void page_stub_72(void);
void page_stub_73(void);
void page_stub_74(void);
void page_stub_75(void);
void page_stub_76(void);
void page_stub_77(void);
void page_stub_78(void);
void page_stub_79(void);
void page_stub_80(void);
void page_stub_81(void);
void page_stub_82(void);
void page_stub_83(void);
void page_stub_84(void);
void page_stub_85(void);
void page_stub_86(void);
void page_stub_87(void);
void page_stub_88(void);
void page_stub_89(void);
void page_stub_90(void);
void page_stub_91(void);
void page_stub_92(void);
void page_stub_93(void);
static void (*const page_stubs[94])(void) = {
    page_stub_0, page_stub_1, page_stub_2, page_stub_3, page_stub_4, page_stub_5,
    page_stub_6, page_stub_7, page_stub_8, page_stub_9, page_stub_10, page_stub_11,
    page_stub_12, page_stub_13, page_stub_14, page_stub_15, page_stub_16, page_stub_17,
    page_stub_18, page_stub_19, page_stub_20, page_stub_21, page_stub_22, page_stub_23,
    page_stub_24, page_stub_25, page_stub_26, page_stub_27, page_stub_28, page_stub_29,
    page_stub_30, page_stub_31, page_stub_32, page_stub_33, page_stub_34, page_stub_35,
    page_stub_36, page_stub_37, page_stub_38, page_stub_39, page_stub_40, page_stub_41,
    page_stub_42, page_stub_43, page_stub_44, page_stub_45, page_stub_46, page_stub_47,
    page_stub_48, page_stub_49, page_stub_50, page_stub_51, page_stub_52, page_stub_53,
    page_stub_54, page_stub_55, page_stub_56, page_stub_57, page_stub_58, page_stub_59,
    page_stub_60, page_stub_61, page_stub_62, page_stub_63, page_stub_64, page_stub_65,
    page_stub_66, page_stub_67, page_stub_68, page_stub_69, page_stub_70, page_stub_71,
    page_stub_72, page_stub_73, page_stub_74, page_stub_75, page_stub_76, page_stub_77,
    page_stub_78, page_stub_79, page_stub_80, page_stub_81, page_stub_82, page_stub_83,
    page_stub_84, page_stub_85, page_stub_86, page_stub_87, page_stub_88, page_stub_89,
    page_stub_90, page_stub_91, page_stub_92, page_stub_93,
};
