2 stdcall SysAllocString(wstr) oleaut32.SysAllocString
3 stdcall SysReAllocString(ptr wstr) oleaut32.SysReAllocString
4 stdcall SysAllocStringLen(wstr long) oleaut32.SysAllocStringLen
5 stdcall SysReAllocStringLen(ptr ptr long) oleaut32.SysReAllocStringLen
6 stdcall SysFreeString(wstr) oleaut32.SysFreeString
7 stdcall SysStringLen(wstr) oleaut32.SysStringLen
8 stdcall VariantInit(ptr) oleaut32.VariantInit
9 stdcall VariantClear(ptr) oleaut32.VariantClear
10 stdcall VariantCopy(ptr ptr) oleaut32.VariantCopy
11 stdcall VariantCopyInd(ptr ptr) oleaut32.VariantCopyInd
12 stdcall VariantChangeType(ptr ptr long long) oleaut32.VariantChangeType
13 stdcall VariantTimeToDosDateTime(double ptr ptr) oleaut32.VariantTimeToDosDateTime
14 stdcall DosDateTimeToVariantTime(long long ptr) oleaut32.DosDateTimeToVariantTime
15 stdcall SafeArrayCreate(long long ptr) oleaut32.SafeArrayCreate
16 stdcall SafeArrayDestroy(ptr) oleaut32.SafeArrayDestroy
17 stdcall SafeArrayGetDim(ptr) oleaut32.SafeArrayGetDim
18 stdcall SafeArrayGetElemsize(ptr) oleaut32.SafeArrayGetElemsize
19 stdcall SafeArrayGetUBound(ptr long long) oleaut32.SafeArrayGetUBound
20 stdcall SafeArrayGetLBound(ptr long long) oleaut32.SafeArrayGetLBound
21 stdcall SafeArrayLock(ptr) oleaut32.SafeArrayLock
22 stdcall SafeArrayUnlock(ptr) oleaut32.SafeArrayUnlock
23 stdcall SafeArrayAccessData(ptr ptr) oleaut32.SafeArrayAccessData
24 stdcall SafeArrayUnaccessData(ptr) oleaut32.SafeArrayUnaccessData
25 stdcall SafeArrayGetElement(ptr ptr ptr) oleaut32.SafeArrayGetElement
26 stdcall SafeArrayPutElement(ptr ptr ptr) oleaut32.SafeArrayPutElement
27 stdcall SafeArrayCopy(ptr ptr) oleaut32.SafeArrayCopy
28 stdcall DispGetParam(ptr long long ptr ptr) oleaut32.DispGetParam
29 stdcall DispGetIDsOfNames(ptr ptr long ptr) oleaut32.DispGetIDsOfNames
30 stdcall DispInvoke(ptr ptr long long ptr ptr ptr ptr) oleaut32.DispInvoke
31 stdcall CreateDispTypeInfo(ptr long ptr) oleaut32.CreateDispTypeInfo
32 stdcall CreateStdDispatch(ptr ptr ptr ptr) oleaut32.CreateStdDispatch
33 stdcall RegisterActiveObject(ptr ptr long ptr) oleaut32.RegisterActiveObject
34 stdcall RevokeActiveObject(long ptr) oleaut32.RevokeActiveObject
35 stdcall GetActiveObject(ptr ptr ptr) oleaut32.GetActiveObject
36 stdcall SafeArrayAllocDescriptor(long ptr) oleaut32.SafeArrayAllocDescriptor
37 stdcall SafeArrayAllocData(ptr) oleaut32.SafeArrayAllocData
38 stdcall SafeArrayDestroyDescriptor(ptr) oleaut32.SafeArrayDestroyDescriptor
39 stdcall SafeArrayDestroyData(ptr) oleaut32.SafeArrayDestroyData
40 stdcall SafeArrayRedim(ptr ptr) oleaut32.SafeArrayRedim
41 stdcall SafeArrayAllocDescriptorEx(long long ptr) oleaut32.SafeArrayAllocDescriptorEx
42 stdcall SafeArrayCreateEx(long long ptr ptr) oleaut32.SafeArrayCreateEx
43 stdcall SafeArrayCreateVectorEx(long long long ptr) oleaut32.SafeArrayCreateVectorEx
44 stdcall SafeArraySetRecordInfo(ptr ptr) oleaut32.SafeArraySetRecordInfo
45 stdcall SafeArrayGetRecordInfo(ptr ptr) oleaut32.SafeArrayGetRecordInfo
46 stdcall VarParseNumFromStr(wstr long long ptr ptr) oleaut32.VarParseNumFromStr
47 stdcall VarNumFromParseNum(ptr ptr long ptr) oleaut32.VarNumFromParseNum
48 stdcall VarI2FromUI1(long ptr) oleaut32.VarI2FromUI1
49 stdcall VarI2FromI4(long ptr) oleaut32.VarI2FromI4
50 stdcall VarI2FromR4(float ptr) oleaut32.VarI2FromR4
51 stdcall VarI2FromR8(double ptr) oleaut32.VarI2FromR8
52 stdcall VarI2FromCy(int64 ptr) oleaut32.VarI2FromCy
53 stdcall VarI2FromDate(double ptr) oleaut32.VarI2FromDate
54 stdcall VarI2FromStr(wstr long long ptr) oleaut32.VarI2FromStr
55 stdcall VarI2FromDisp(ptr long ptr) oleaut32.VarI2FromDisp
56 stdcall VarI2FromBool(long ptr) oleaut32.VarI2FromBool
57 stdcall SafeArraySetIID(ptr ptr) oleaut32.SafeArraySetIID
58 stdcall VarI4FromUI1(long ptr) oleaut32.VarI4FromUI1
59 stdcall VarI4FromI2(long ptr) oleaut32.VarI4FromI2
60 stdcall VarI4FromR4(float ptr) oleaut32.VarI4FromR4
61 stdcall VarI4FromR8(double ptr) oleaut32.VarI4FromR8
62 stdcall VarI4FromCy(int64 ptr) oleaut32.VarI4FromCy
63 stdcall VarI4FromDate(double ptr) oleaut32.VarI4FromDate
64 stdcall VarI4FromStr(wstr long long ptr) oleaut32.VarI4FromStr
65 stdcall VarI4FromDisp(ptr long ptr) oleaut32.VarI4FromDisp
66 stdcall VarI4FromBool(long ptr) oleaut32.VarI4FromBool
67 stdcall SafeArrayGetIID(ptr ptr) oleaut32.SafeArrayGetIID
68 stdcall VarR4FromUI1(long ptr) oleaut32.VarR4FromUI1
69 stdcall VarR4FromI2(long ptr) oleaut32.VarR4FromI2
70 stdcall VarR4FromI4(long ptr) oleaut32.VarR4FromI4
71 stdcall VarR4FromR8(double ptr) oleaut32.VarR4FromR8
72 stdcall VarR4FromCy(int64 ptr) oleaut32.VarR4FromCy
73 stdcall VarR4FromDate(double ptr) oleaut32.VarR4FromDate
74 stdcall VarR4FromStr(wstr long long ptr) oleaut32.VarR4FromStr
75 stdcall VarR4FromDisp(ptr long ptr) oleaut32.VarR4FromDisp
76 stdcall VarR4FromBool(long ptr) oleaut32.VarR4FromBool
77 stdcall SafeArrayGetVartype(ptr ptr) oleaut32.SafeArrayGetVartype
78 stdcall VarR8FromUI1(long ptr) oleaut32.VarR8FromUI1
79 stdcall VarR8FromI2(long ptr) oleaut32.VarR8FromI2
80 stdcall VarR8FromI4(long ptr) oleaut32.VarR8FromI4
81 stdcall VarR8FromR4(float ptr) oleaut32.VarR8FromR4
82 stdcall VarR8FromCy(int64 ptr) oleaut32.VarR8FromCy
83 stdcall VarR8FromDate(double ptr) oleaut32.VarR8FromDate
84 stdcall VarR8FromStr(wstr long long ptr) oleaut32.VarR8FromStr
85 stdcall VarR8FromDisp(ptr long ptr) oleaut32.VarR8FromDisp
86 stdcall VarR8FromBool(long ptr) oleaut32.VarR8FromBool
87 stdcall VarFormat(ptr ptr long long long ptr) oleaut32.VarFormat
88 stdcall VarDateFromUI1(long ptr) oleaut32.VarDateFromUI1
89 stdcall VarDateFromI2(long ptr) oleaut32.VarDateFromI2
90 stdcall VarDateFromI4(long ptr) oleaut32.VarDateFromI4
91 stdcall VarDateFromR4(float ptr) oleaut32.VarDateFromR4
92 stdcall VarDateFromR8(double ptr) oleaut32.VarDateFromR8
93 stdcall VarDateFromCy(int64 ptr) oleaut32.VarDateFromCy
94 stdcall VarDateFromStr(wstr long long ptr) oleaut32.VarDateFromStr
95 stdcall VarDateFromDisp(ptr long ptr) oleaut32.VarDateFromDisp
96 stdcall VarDateFromBool(long ptr) oleaut32.VarDateFromBool
97 stdcall VarFormatDateTime(ptr long long ptr) oleaut32.VarFormatDateTime
98 stdcall VarCyFromUI1(long ptr) oleaut32.VarCyFromUI1
99 stdcall VarCyFromI2(long ptr) oleaut32.VarCyFromI2
100 stdcall VarCyFromI4(long ptr) oleaut32.VarCyFromI4
101 stdcall VarCyFromR4(float ptr) oleaut32.VarCyFromR4
102 stdcall VarCyFromR8(double ptr) oleaut32.VarCyFromR8
103 stdcall VarCyFromDate(double ptr) oleaut32.VarCyFromDate
104 stdcall VarCyFromStr(wstr long long ptr) oleaut32.VarCyFromStr
105 stdcall VarCyFromDisp(ptr long ptr) oleaut32.VarCyFromDisp
106 stdcall VarCyFromBool(long ptr) oleaut32.VarCyFromBool
107 stdcall VarFormatNumber(ptr long long long long long ptr) oleaut32.VarFormatNumber
108 stdcall VarBstrFromUI1(long long long ptr) oleaut32.VarBstrFromUI1
109 stdcall VarBstrFromI2(long long long ptr) oleaut32.VarBstrFromI2
110 stdcall VarBstrFromI4(long long long ptr) oleaut32.VarBstrFromI4
111 stdcall VarBstrFromR4(float long long ptr) oleaut32.VarBstrFromR4
112 stdcall VarBstrFromR8(double long long ptr) oleaut32.VarBstrFromR8
113 stdcall VarBstrFromCy(int64 long long ptr) oleaut32.VarBstrFromCy
114 stdcall VarBstrFromDate(double long long ptr) oleaut32.VarBstrFromDate
115 stdcall VarBstrFromDisp(ptr long long ptr) oleaut32.VarBstrFromDisp
116 stdcall VarBstrFromBool(long long long ptr) oleaut32.VarBstrFromBool
117 stdcall VarFormatPercent(ptr long long long long long ptr) oleaut32.VarFormatPercent
118 stdcall VarBoolFromUI1(long ptr) oleaut32.VarBoolFromUI1
119 stdcall VarBoolFromI2(long ptr) oleaut32.VarBoolFromI2
120 stdcall VarBoolFromI4(long ptr) oleaut32.VarBoolFromI4
121 stdcall VarBoolFromR4(float ptr) oleaut32.VarBoolFromR4
122 stdcall VarBoolFromR8(double ptr) oleaut32.VarBoolFromR8
123 stdcall VarBoolFromDate(double ptr) oleaut32.VarBoolFromDate
124 stdcall VarBoolFromCy(int64 ptr) oleaut32.VarBoolFromCy
125 stdcall VarBoolFromStr(wstr long long ptr) oleaut32.VarBoolFromStr
126 stdcall VarBoolFromDisp(ptr long ptr) oleaut32.VarBoolFromDisp
127 stdcall VarFormatCurrency(ptr long long long long long ptr) oleaut32.VarFormatCurrency
128 stdcall VarWeekdayName(long long long long ptr) oleaut32.VarWeekdayName
129 stdcall VarMonthName(long long long ptr) oleaut32.VarMonthName
130 stdcall VarUI1FromI2(long ptr) oleaut32.VarUI1FromI2
131 stdcall VarUI1FromI4(long ptr) oleaut32.VarUI1FromI4
132 stdcall VarUI1FromR4(float ptr) oleaut32.VarUI1FromR4
133 stdcall VarUI1FromR8(double ptr) oleaut32.VarUI1FromR8
134 stdcall VarUI1FromCy(int64 ptr) oleaut32.VarUI1FromCy
135 stdcall VarUI1FromDate(double ptr) oleaut32.VarUI1FromDate
136 stdcall VarUI1FromStr(wstr long long ptr) oleaut32.VarUI1FromStr
137 stdcall VarUI1FromDisp(ptr long ptr) oleaut32.VarUI1FromDisp
138 stdcall VarUI1FromBool(long ptr) oleaut32.VarUI1FromBool
139 stdcall VarFormatFromTokens (ptr ptr ptr long ptr long) oleaut32.VarFormatFromTokens
140 stdcall VarTokenizeFormatString (ptr ptr long long long long ptr) oleaut32.VarTokenizeFormatString
141 stdcall VarAdd(ptr ptr ptr) oleaut32.VarAdd
142 stdcall VarAnd(ptr ptr ptr) oleaut32.VarAnd
143 stdcall VarDiv(ptr ptr ptr) oleaut32.VarDiv
144 stub OACreateTypeLib2
146 stdcall DispCallFunc(ptr long long long long ptr ptr ptr) oleaut32.DispCallFunc
147 stdcall VariantChangeTypeEx(ptr ptr long long long) oleaut32.VariantChangeTypeEx
148 stdcall SafeArrayPtrOfIndex(ptr ptr ptr) oleaut32.SafeArrayPtrOfIndex
149 stdcall SysStringByteLen(ptr) oleaut32.SysStringByteLen
150 stdcall SysAllocStringByteLen(ptr long) oleaut32.SysAllocStringByteLen
152 stdcall VarEqv(ptr ptr ptr) oleaut32.VarEqv
153 stdcall VarIdiv(ptr ptr ptr) oleaut32.VarIdiv
154 stdcall VarImp(ptr ptr ptr) oleaut32.VarImp
155 stdcall VarMod(ptr ptr ptr) oleaut32.VarMod
156 stdcall VarMul(ptr ptr ptr) oleaut32.VarMul
157 stdcall VarOr(ptr ptr ptr) oleaut32.VarOr
158 stdcall VarPow(ptr ptr ptr) oleaut32.VarPow
159 stdcall VarSub(ptr ptr ptr) oleaut32.VarSub
160 stdcall CreateTypeLib(long wstr ptr) oleaut32.CreateTypeLib
161 stdcall LoadTypeLib (wstr ptr) oleaut32.LoadTypeLib
162 stdcall LoadRegTypeLib (ptr long long long ptr) oleaut32.LoadRegTypeLib
163 stdcall RegisterTypeLib(ptr wstr wstr) oleaut32.RegisterTypeLib
164 stdcall QueryPathOfRegTypeLib(ptr long long long ptr) oleaut32.QueryPathOfRegTypeLib
165 stdcall LHashValOfNameSys(long long wstr) oleaut32.LHashValOfNameSys
166 stdcall LHashValOfNameSysA(long long str) oleaut32.LHashValOfNameSysA
167 stdcall VarXor(ptr ptr ptr) oleaut32.VarXor
168 stdcall VarAbs(ptr ptr) oleaut32.VarAbs
169 stdcall VarFix(ptr ptr) oleaut32.VarFix
170 stdcall OaBuildVersion() oleaut32.OaBuildVersion
171 stdcall ClearCustData(ptr) oleaut32.ClearCustData
172 stdcall VarInt(ptr ptr) oleaut32.VarInt
173 stdcall VarNeg(ptr ptr) oleaut32.VarNeg
174 stdcall VarNot(ptr ptr) oleaut32.VarNot
175 stdcall VarRound(ptr long ptr) oleaut32.VarRound
176 stdcall VarCmp(ptr ptr long long) oleaut32.VarCmp
177 stdcall VarDecAdd(ptr ptr ptr) oleaut32.VarDecAdd
178 stdcall VarDecDiv(ptr ptr ptr) oleaut32.VarDecDiv
179 stdcall VarDecMul(ptr ptr ptr) oleaut32.VarDecMul
180 stdcall CreateTypeLib2(long wstr ptr) oleaut32.CreateTypeLib2
181 stdcall VarDecSub(ptr ptr ptr) oleaut32.VarDecSub
182 stdcall VarDecAbs(ptr ptr) oleaut32.VarDecAbs
183 stdcall LoadTypeLibEx (wstr long ptr) oleaut32.LoadTypeLibEx
184 stdcall SystemTimeToVariantTime(ptr ptr) oleaut32.SystemTimeToVariantTime
185 stdcall VariantTimeToSystemTime(double ptr) oleaut32.VariantTimeToSystemTime
186 stdcall UnRegisterTypeLib (ptr long long long long) oleaut32.UnRegisterTypeLib
187 stdcall VarDecFix(ptr ptr) oleaut32.VarDecFix
188 stdcall VarDecInt(ptr ptr) oleaut32.VarDecInt
189 stdcall VarDecNeg(ptr ptr) oleaut32.VarDecNeg
190 stdcall VarDecFromUI1(long ptr) oleaut32.VarDecFromUI1
191 stdcall VarDecFromI2(long ptr) oleaut32.VarDecFromI2
192 stdcall VarDecFromI4(long ptr) oleaut32.VarDecFromI4
193 stdcall VarDecFromR4(float ptr) oleaut32.VarDecFromR4
194 stdcall VarDecFromR8(double ptr) oleaut32.VarDecFromR8
195 stdcall VarDecFromDate(double ptr) oleaut32.VarDecFromDate
196 stdcall VarDecFromCy(int64 ptr) oleaut32.VarDecFromCy
197 stdcall VarDecFromStr(wstr long long ptr) oleaut32.VarDecFromStr
198 stdcall VarDecFromDisp(ptr long ptr) oleaut32.VarDecFromDisp
199 stdcall VarDecFromBool(long ptr) oleaut32.VarDecFromBool
200 stdcall GetErrorInfo(long ptr) ole32.GetErrorInfo
201 stdcall SetErrorInfo(long ptr) ole32.SetErrorInfo
202 stdcall CreateErrorInfo(ptr) ole32.CreateErrorInfo
203 stdcall VarDecRound(ptr long ptr) oleaut32.VarDecRound
204 stdcall VarDecCmp(ptr ptr) oleaut32.VarDecCmp
205 stdcall VarI2FromI1(long ptr) oleaut32.VarI2FromI1
206 stdcall VarI2FromUI2(long ptr) oleaut32.VarI2FromUI2
207 stdcall VarI2FromUI4(long ptr) oleaut32.VarI2FromUI4
208 stdcall VarI2FromDec(ptr ptr) oleaut32.VarI2FromDec
209 stdcall VarI4FromI1(long ptr) oleaut32.VarI4FromI1
210 stdcall VarI4FromUI2(long ptr) oleaut32.VarI4FromUI2
211 stdcall VarI4FromUI4(long ptr) oleaut32.VarI4FromUI4
212 stdcall VarI4FromDec(ptr ptr) oleaut32.VarI4FromDec
213 stdcall VarR4FromI1(long ptr) oleaut32.VarR4FromI1
214 stdcall VarR4FromUI2(long ptr) oleaut32.VarR4FromUI2
215 stdcall VarR4FromUI4(long ptr) oleaut32.VarR4FromUI4
216 stdcall VarR4FromDec(ptr ptr) oleaut32.VarR4FromDec
217 stdcall VarR8FromI1(long ptr) oleaut32.VarR8FromI1
218 stdcall VarR8FromUI2(long ptr) oleaut32.VarR8FromUI2
219 stdcall VarR8FromUI4(long ptr) oleaut32.VarR8FromUI4
220 stdcall VarR8FromDec(ptr ptr) oleaut32.VarR8FromDec
221 stdcall VarDateFromI1(long ptr) oleaut32.VarDateFromI1
222 stdcall VarDateFromUI2(long ptr) oleaut32.VarDateFromUI2
223 stdcall VarDateFromUI4(long ptr) oleaut32.VarDateFromUI4
224 stdcall VarDateFromDec(ptr ptr) oleaut32.VarDateFromDec
225 stdcall VarCyFromI1(long ptr) oleaut32.VarCyFromI1
226 stdcall VarCyFromUI2(long ptr) oleaut32.VarCyFromUI2
227 stdcall VarCyFromUI4(long ptr) oleaut32.VarCyFromUI4
228 stdcall VarCyFromDec(ptr ptr) oleaut32.VarCyFromDec
229 stdcall VarBstrFromI1(long long long ptr) oleaut32.VarBstrFromI1
230 stdcall VarBstrFromUI2(long long long ptr) oleaut32.VarBstrFromUI2
231 stdcall VarBstrFromUI4(long long long ptr) oleaut32.VarBstrFromUI4
232 stdcall VarBstrFromDec(ptr long long ptr) oleaut32.VarBstrFromDec
233 stdcall VarBoolFromI1(long ptr) oleaut32.VarBoolFromI1
234 stdcall VarBoolFromUI2(long ptr) oleaut32.VarBoolFromUI2
235 stdcall VarBoolFromUI4(long ptr) oleaut32.VarBoolFromUI4
236 stdcall VarBoolFromDec(ptr ptr) oleaut32.VarBoolFromDec
237 stdcall VarUI1FromI1(long ptr) oleaut32.VarUI1FromI1
238 stdcall VarUI1FromUI2(long ptr) oleaut32.VarUI1FromUI2
239 stdcall VarUI1FromUI4(long ptr) oleaut32.VarUI1FromUI4
240 stdcall VarUI1FromDec(ptr ptr) oleaut32.VarUI1FromDec
241 stdcall VarDecFromI1(long ptr) oleaut32.VarDecFromI1
242 stdcall VarDecFromUI2(long ptr) oleaut32.VarDecFromUI2
243 stdcall VarDecFromUI4(long ptr) oleaut32.VarDecFromUI4
244 stdcall VarI1FromUI1(long ptr) oleaut32.VarI1FromUI1
245 stdcall VarI1FromI2(long ptr) oleaut32.VarI1FromI2
246 stdcall VarI1FromI4(long ptr) oleaut32.VarI1FromI4
247 stdcall VarI1FromR4(float ptr) oleaut32.VarI1FromR4
248 stdcall VarI1FromR8(double ptr) oleaut32.VarI1FromR8
249 stdcall VarI1FromDate(double ptr) oleaut32.VarI1FromDate
250 stdcall VarI1FromCy(int64 ptr) oleaut32.VarI1FromCy
251 stdcall VarI1FromStr(wstr long long ptr) oleaut32.VarI1FromStr
252 stdcall VarI1FromDisp(ptr long ptr) oleaut32.VarI1FromDisp
253 stdcall VarI1FromBool(long ptr) oleaut32.VarI1FromBool
254 stdcall VarI1FromUI2(long ptr) oleaut32.VarI1FromUI2
255 stdcall VarI1FromUI4(long ptr) oleaut32.VarI1FromUI4
256 stdcall VarI1FromDec(ptr ptr) oleaut32.VarI1FromDec
257 stdcall VarUI2FromUI1(long ptr) oleaut32.VarUI2FromUI1
258 stdcall VarUI2FromI2(long ptr) oleaut32.VarUI2FromI2
259 stdcall VarUI2FromI4(long ptr) oleaut32.VarUI2FromI4
260 stdcall VarUI2FromR4(float ptr) oleaut32.VarUI2FromR4
261 stdcall VarUI2FromR8(double ptr) oleaut32.VarUI2FromR8
262 stdcall VarUI2FromDate(double ptr) oleaut32.VarUI2FromDate
263 stdcall VarUI2FromCy(int64 ptr) oleaut32.VarUI2FromCy
264 stdcall VarUI2FromStr(wstr long long ptr) oleaut32.VarUI2FromStr
265 stdcall VarUI2FromDisp(ptr long ptr) oleaut32.VarUI2FromDisp
266 stdcall VarUI2FromBool(long ptr) oleaut32.VarUI2FromBool
267 stdcall VarUI2FromI1(long ptr) oleaut32.VarUI2FromI1
268 stdcall VarUI2FromUI4(long ptr) oleaut32.VarUI2FromUI4
269 stdcall VarUI2FromDec(ptr ptr) oleaut32.VarUI2FromDec
270 stdcall VarUI4FromUI1(long ptr) oleaut32.VarUI4FromUI1
271 stdcall VarUI4FromI2(long ptr) oleaut32.VarUI4FromI2
272 stdcall VarUI4FromI4(long ptr) oleaut32.VarUI4FromI4
273 stdcall VarUI4FromR4(float ptr) oleaut32.VarUI4FromR4
274 stdcall VarUI4FromR8(double ptr) oleaut32.VarUI4FromR8
275 stdcall VarUI4FromDate(double ptr) oleaut32.VarUI4FromDate
276 stdcall VarUI4FromCy(int64 ptr) oleaut32.VarUI4FromCy
277 stdcall VarUI4FromStr(wstr long long ptr) oleaut32.VarUI4FromStr
278 stdcall VarUI4FromDisp(ptr long ptr) oleaut32.VarUI4FromDisp
279 stdcall VarUI4FromBool(long ptr) oleaut32.VarUI4FromBool
280 stdcall VarUI4FromI1(long ptr) oleaut32.VarUI4FromI1
281 stdcall VarUI4FromUI2(long ptr) oleaut32.VarUI4FromUI2
282 stdcall VarUI4FromDec(ptr ptr) oleaut32.VarUI4FromDec
283 stdcall BSTR_UserSize(ptr long ptr) oleaut32.BSTR_UserSize
284 stdcall BSTR_UserMarshal(ptr ptr ptr) oleaut32.BSTR_UserMarshal
285 stdcall BSTR_UserUnmarshal(ptr ptr ptr) oleaut32.BSTR_UserUnmarshal
286 stdcall BSTR_UserFree(ptr ptr) oleaut32.BSTR_UserFree
287 stdcall VARIANT_UserSize(ptr long ptr) oleaut32.VARIANT_UserSize
288 stdcall VARIANT_UserMarshal(ptr ptr ptr) oleaut32.VARIANT_UserMarshal
289 stdcall VARIANT_UserUnmarshal(ptr ptr ptr) oleaut32.VARIANT_UserUnmarshal
290 stdcall VARIANT_UserFree(ptr ptr) oleaut32.VARIANT_UserFree
291 stdcall LPSAFEARRAY_UserSize(ptr long ptr) oleaut32.LPSAFEARRAY_UserSize
292 stdcall LPSAFEARRAY_UserMarshal(ptr ptr ptr) oleaut32.LPSAFEARRAY_UserMarshal
293 stdcall LPSAFEARRAY_UserUnmarshal(ptr ptr ptr) oleaut32.LPSAFEARRAY_UserUnmarshal
294 stdcall LPSAFEARRAY_UserFree(ptr ptr) oleaut32.LPSAFEARRAY_UserFree
295 stub LPSAFEARRAY_Size
296 stub LPSAFEARRAY_Marshal
297 stub LPSAFEARRAY_Unmarshal
298 stdcall VarDecCmpR8(ptr double) oleaut32.VarDecCmpR8
299 stdcall VarCyAdd(int64 int64 ptr) oleaut32.VarCyAdd
303 stdcall VarCyMul(int64 int64 ptr) oleaut32.VarCyMul
304 stdcall VarCyMulI4(int64 long ptr) oleaut32.VarCyMulI4
305 stdcall VarCySub(int64 int64 ptr) oleaut32.VarCySub
306 stdcall VarCyAbs(int64 ptr) oleaut32.VarCyAbs
307 stdcall VarCyFix(int64 ptr) oleaut32.VarCyFix
308 stdcall VarCyInt(int64 ptr) oleaut32.VarCyInt
309 stdcall VarCyNeg(int64 ptr) oleaut32.VarCyNeg
310 stdcall VarCyRound(int64 long ptr) oleaut32.VarCyRound
311 stdcall VarCyCmp(int64 int64) oleaut32.VarCyCmp
312 stdcall VarCyCmpR8(int64 double) oleaut32.VarCyCmpR8
313 stdcall VarBstrCat(wstr wstr ptr) oleaut32.VarBstrCat
314 stdcall VarBstrCmp(wstr wstr long long) oleaut32.VarBstrCmp
315 stdcall VarR8Pow(double double ptr) oleaut32.VarR8Pow
316 stdcall VarR4CmpR8(float double) oleaut32.VarR4CmpR8
317 stdcall VarR8Round(double long ptr) oleaut32.VarR8Round
318 stdcall VarCat(ptr ptr ptr) oleaut32.VarCat
319 stdcall VarDateFromUdateEx(ptr long long ptr) oleaut32.VarDateFromUdateEx
322 stdcall GetRecordInfoFromGuids(ptr long long long ptr ptr) oleaut32.GetRecordInfoFromGuids
323 stdcall GetRecordInfoFromTypeInfo(ptr ptr) oleaut32.GetRecordInfoFromTypeInfo
325 stub SetVarConversionLocaleSetting
326 stub GetVarConversionLocaleSetting
327 stdcall SetOaNoCache() oleaut32.SetOaNoCache
329 stdcall VarCyMulI8(int64 int64 ptr) oleaut32.VarCyMulI8
330 stdcall VarDateFromUdate(ptr long ptr) oleaut32.VarDateFromUdate
331 stdcall VarUdateFromDate(double long ptr) oleaut32.VarUdateFromDate
332 stub GetAltMonthNames
333 stdcall VarI8FromUI1(long long) oleaut32.VarI8FromUI1
334 stdcall VarI8FromI2(long long) oleaut32.VarI8FromI2
335 stdcall VarI8FromR4(float long) oleaut32.VarI8FromR4
336 stdcall VarI8FromR8(double long) oleaut32.VarI8FromR8
337 stdcall VarI8FromCy(int64 ptr) oleaut32.VarI8FromCy
338 stdcall VarI8FromDate(double long) oleaut32.VarI8FromDate
339 stdcall VarI8FromStr(wstr long long ptr) oleaut32.VarI8FromStr
340 stdcall VarI8FromDisp(ptr long ptr) oleaut32.VarI8FromDisp
341 stdcall VarI8FromBool(long long) oleaut32.VarI8FromBool
342 stdcall VarI8FromI1(long long) oleaut32.VarI8FromI1
343 stdcall VarI8FromUI2(long long) oleaut32.VarI8FromUI2
344 stdcall VarI8FromUI4(long long) oleaut32.VarI8FromUI4
345 stdcall VarI8FromDec(ptr ptr) oleaut32.VarI8FromDec
346 stdcall VarI2FromI8(int64 ptr) oleaut32.VarI2FromI8
347 stdcall VarI2FromUI8(int64 ptr) oleaut32.VarI2FromUI8
348 stdcall VarI4FromI8(int64 ptr) oleaut32.VarI4FromI8
349 stdcall VarI4FromUI8(int64 ptr) oleaut32.VarI4FromUI8
360 stdcall VarR4FromI8(int64 ptr) oleaut32.VarR4FromI8
361 stdcall VarR4FromUI8(int64 ptr) oleaut32.VarR4FromUI8
362 stdcall VarR8FromI8(int64 ptr) oleaut32.VarR8FromI8
363 stdcall VarR8FromUI8(int64 ptr) oleaut32.VarR8FromUI8
364 stdcall VarDateFromI8(int64 ptr) oleaut32.VarDateFromI8
365 stdcall VarDateFromUI8(int64 ptr) oleaut32.VarDateFromUI8
366 stdcall VarCyFromI8(int64 ptr) oleaut32.VarCyFromI8
367 stdcall VarCyFromUI8(int64 ptr) oleaut32.VarCyFromUI8
368 stdcall VarBstrFromI8(int64 long long ptr) oleaut32.VarBstrFromI8
369 stdcall VarBstrFromUI8(int64 long long ptr) oleaut32.VarBstrFromUI8
370 stdcall VarBoolFromI8(int64 ptr) oleaut32.VarBoolFromI8
371 stdcall VarBoolFromUI8(int64 ptr) oleaut32.VarBoolFromUI8
372 stdcall VarUI1FromI8(int64 ptr) oleaut32.VarUI1FromI8
373 stdcall VarUI1FromUI8(int64 ptr) oleaut32.VarUI1FromUI8
374 stdcall VarDecFromI8(int64 ptr) oleaut32.VarDecFromI8
375 stdcall VarDecFromUI8(int64 ptr) oleaut32.VarDecFromUI8
376 stdcall VarI1FromI8(int64 ptr) oleaut32.VarI1FromI8
377 stdcall VarI1FromUI8(int64 ptr) oleaut32.VarI1FromUI8
378 stdcall VarUI2FromI8(int64 ptr) oleaut32.VarUI2FromI8
379 stdcall VarUI2FromUI8(int64 ptr) oleaut32.VarUI2FromUI8
380 stub UserHWND_from_local
381 stub UserHWND_to_local
382 stub UserHWND_free_inst
383 stub UserHWND_free_local
384 stub UserBSTR_from_local
385 stub UserBSTR_to_local
386 stub UserBSTR_free_inst
387 stub UserBSTR_free_local
388 stub UserVARIANT_from_local
389 stub UserVARIANT_to_local
390 stub UserVARIANT_free_inst
391 stub UserVARIANT_free_local
392 stub UserEXCEPINFO_from_local
393 stub UserEXCEPINFO_to_local
394 stub UserEXCEPINFO_free_inst
395 stub UserEXCEPINFO_free_local
396 stub UserMSG_from_local
397 stub UserMSG_to_local
398 stub UserMSG_free_inst
399 stub UserMSG_free_local
401 stdcall OleLoadPictureEx(ptr long long long long long long ptr) oleaut32.OleLoadPictureEx
402 stub OleLoadPictureFileEx
411 stdcall SafeArrayCreateVector(long long long) oleaut32.SafeArrayCreateVector
412 stdcall SafeArrayCopyData(ptr ptr) oleaut32.SafeArrayCopyData
413 stdcall VectorFromBstr(ptr ptr) oleaut32.VectorFromBstr
414 stdcall BstrFromVector(ptr ptr) oleaut32.BstrFromVector
415 stdcall OleIconToCursor(long long) oleaut32.OleIconToCursor
416 stdcall OleCreatePropertyFrameIndirect(ptr) oleaut32.OleCreatePropertyFrameIndirect
417 stdcall OleCreatePropertyFrame(ptr long long ptr long ptr long ptr ptr long ptr) oleaut32.OleCreatePropertyFrame
418 stdcall OleLoadPicture(ptr long long ptr ptr) oleaut32.OleLoadPicture
419 stdcall OleCreatePictureIndirect(ptr ptr long ptr) oleaut32.OleCreatePictureIndirect
420 stdcall OleCreateFontIndirect(ptr ptr ptr) oleaut32.OleCreateFontIndirect
421 stdcall OleTranslateColor(long long long) oleaut32.OleTranslateColor
422 stub OleLoadPictureFile
423 stub OleSavePictureFile
424 stdcall OleLoadPicturePath(wstr ptr long long ptr ptr) oleaut32.OleLoadPicturePath
425 stdcall VarUI4FromI8(int64 ptr) oleaut32.VarUI4FromI8
426 stdcall VarUI4FromUI8(int64 ptr) oleaut32.VarUI4FromUI8
427 stdcall VarI8FromUI8(int64 ptr) oleaut32.VarI8FromUI8
428 stdcall VarUI8FromI8(int64 ptr) oleaut32.VarUI8FromI8
429 stdcall VarUI8FromUI1(long ptr) oleaut32.VarUI8FromUI1
430 stdcall VarUI8FromI2(long ptr) oleaut32.VarUI8FromI2
431 stdcall VarUI8FromR4(float ptr) oleaut32.VarUI8FromR4
432 stdcall VarUI8FromR8(double ptr) oleaut32.VarUI8FromR8
433 stdcall VarUI8FromCy(int64 ptr) oleaut32.VarUI8FromCy
434 stdcall VarUI8FromDate(double ptr) oleaut32.VarUI8FromDate
435 stdcall VarUI8FromStr(wstr long long ptr) oleaut32.VarUI8FromStr
436 stdcall VarUI8FromDisp(ptr long ptr) oleaut32.VarUI8FromDisp
437 stdcall VarUI8FromBool(long ptr) oleaut32.VarUI8FromBool
438 stdcall VarUI8FromI1(long ptr) oleaut32.VarUI8FromI1
439 stdcall VarUI8FromUI2(long ptr) oleaut32.VarUI8FromUI2
440 stdcall VarUI8FromUI4(long ptr) oleaut32.VarUI8FromUI4
441 stdcall VarUI8FromDec(long ptr) oleaut32.VarUI8FromDec
442 stdcall RegisterTypeLibForUser(ptr wstr wstr) oleaut32.RegisterTypeLibForUser
443 stdcall UnRegisterTypeLibForUser(ptr long long long long) oleaut32.UnRegisterTypeLibForUser

@ stdcall -private DllCanUnloadNow() oleaut32.DllCanUnloadNow
@ stdcall -private DllGetClassObject(ptr ptr ptr) oleaut32.DllGetClassObject
@ stdcall -private DllRegisterServer() oleaut32.DllRegisterServer
@ stdcall -private DllUnregisterServer() oleaut32.DllUnregisterServer
