from pathlib import Path
from html import escape
import sys
D=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).parent/'staged/source/images/velox-data-source-main'
D.mkdir(parents=True,exist_ok=True)
C={'blue':('#4078bd','#f3f7fc','#326cae'),'green':('#39826d','#f0f7f3','#2e7963'),'purple':('#8d76b7','#f7f4fb','#65537f'),'warm':('#ac9c76','#faf7ef','#877951'),'gray':('#657386','#ffffff','#657386'),'red':('#a95f64','#fbf3f3','#8d4f54')}
class SVG:
 def __init__(self,title,subtitle,h):
  self.parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="{h}" viewBox="0 0 1280 {h}" role="img"><title>{escape(title)}</title><desc>{escape(subtitle)}</desc><rect width="1280" height="{h}" fill="#ffffff"/><defs>']
  for key,(stroke,_,_) in C.items():self.parts.append(f'<marker id="arrow-{key}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0 L10 5 L0 10 Z" fill="{stroke}"/></marker>')
  self.parts.append('</defs>');self.t(32,45,title,28,'#1f2d3d',True);self.t(32,79,subtitle,19);self.line([(32,101),(1248,101)],arrow=False,color='#dce3ea')
 def t(self,x,y,s,size=20,color='#657386',bold=False,anchor='start',mono=False):
  font='DejaVu Sans Mono,monospace' if mono else 'Noto Sans SC,Noto Sans CJK SC,Arial,sans-serif'
  self.parts.append(f'<text x="{x}" y="{y}" font-family="{font}" font-size="{size}" font-weight="{600 if bold else 400}" fill="{color}" text-anchor="{anchor}">{escape(str(s))}</text>')
 def rect(self,x,y,w,h,key='blue',fill=None):
  self.parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" fill="{fill or C[key][1]}" stroke="{C[key][0]}" stroke-width="1.5"/>')
 def box(self,x,y,w,h,title,lines=(),key='blue',size=22):
  self.rect(x,y,w,h,key);self.t(x+w/2,y+30,title,size,C[key][2],True,'middle')
  for i,line in enumerate(lines):self.t(x+w/2,y+59+i*28,line,19,'#1f2d3d',anchor='middle')
 def line(self,pts,key='gray',arrow=True,dashed=False,width=1.8,color=None):
  attr=f' marker-end="url(#arrow-{key})"' if arrow else ''
  if dashed:attr+=' stroke-dasharray="6 5"'
  self.parts.append('<polyline points="'+' '.join(f'{x},{y}' for x,y in pts)+f'" fill="none" stroke="{color or C[key][0]}" stroke-width="{width}" stroke-linejoin="round"{attr}/>')
 def note(self,y,lines):
  self.line([(32,y-26),(1248,y-26)],arrow=False,color='#dce3ea')
  for i,line in enumerate(lines):self.t(32,y+i*29,line,20)
 def save(self,n): (D/f'{n}.svg').write_text('\n'.join(self.parts)+'\n</svg>')

s=SVG('文件扫描：执行接口、DataSource 与 DWIO','实线表示调用 / 持有；虚线箭头指向基类。Hive 与 Iceberg 各有 Connector 工厂。',775)
s.box(32,130,292,92,'TableScanNode',['outputType · handles'],'blue')
s.box(32,266,292,92,'TableScan',['Task split → next batch'],'blue');s.line([(178,222),(178,266)],'blue')
s.box(384,130,382,92,'HiveConnector',['create HiveDataSource'],'green')
s.box(816,130,432,92,'IcebergConnector',['create IcebergDataSource'],'purple')
s.line([(324,304),(355,304),(355,176),(384,176)],'green')
s.line([(324,329),(790,329),(790,176),(816,176)],'purple')
s.box(400,252,350,58,'HiveDataSource',key='green');s.box(862,252,350,58,'IcebergDataSource',key='purple')
s.line([(575,222),(575,252)],'green');s.line([(1032,222),(1032,252)],'purple');s.line([(862,282),(750,282)],'purple',dashed=True)
s.box(440,378,404,86,'FileDataSource',['ScanSpec · ExprSet · output_'],'blue')
s.line([(575,310),(575,378)],'green',dashed=True)
s.t(877,399,'保持计划输出前缀',20);s.t(877,431,'管理 split / batch 生命周期',20)
s.box(32,385,292,82,'ConnectorQueryCtx',['evaluator · pools · config'],'warm');s.line([(440,420),(324,420)],'warm')
s.box(228,522,350,80,'HiveSplitReader',['分区 / 缺列 · 桶筛选'],'green');s.box(738,522,380,80,'IcebergSplitReader',['ID / 默认值 · deletes / lineage'],'purple')
s.line([(550,464),(550,493),(403,493),(403,522)],'green');s.line([(735,464),(735,493),(928,493),(928,522)],'purple')
s.box(478,632,336,56,'FileSplitReader',key='blue')
s.line([(403,602),(403,660),(478,660)],'green',dashed=True);s.line([(928,602),(928,660),(814,660)],'purple',dashed=True)
s.t(32,730,'FileSplitReader 持有：',21,'#1f2d3d',True);s.t(300,730,'Reader → RowReader → 格式 ColumnReader 树',21,C['blue'][2],True);s.line([(646,688),(646,707)],'blue')
s.save('architecture')

s=SVG('一次 batch：三层行坐标，两个过滤阶段','c2 只在 reader 内过滤；c1 是 remaining expression 的辅助输出；最终恢复计划名 x。',645)
# column-major row table
s.t(32,143,'文件原始行',23,C['blue'][2],True)
headers=['row','c0','c1','c2'];vals=[[0,4,8,1],[1,5,9,-1],[2,8,1,2],[3,9,3,3],[4,3,2,-2]]
for j,h in enumerate(headers):s.box(32+j*81,168,77,51,h,key='blue',size=20)
for i,row in enumerate(vals):
 for j,v in enumerate(row):
  k='blue' if i in [0,2,3] else 'warm';s.rect(32+j*81,225+i*48,77,42,k);s.t(70+j*81,253+i*48,v,20,C[k][2],anchor='middle',mono=True)
s.t(406,173,'c2 ≥ 0',22,C['purple'][2],True);s.line([(356,318),(483,318)],'purple')
s.t(493,143,'reader 输出 R',23,C['green'][2],True)
for i,(row,c0,c1) in enumerate([(0,4,8),(2,8,1),(3,9,3)]):
 s.box(493,180+i*97,334,75,f'位置 {i} ← 文件行 {row}',[f'c0={c0}    p=east    c1={c1}'],'green',20)
s.t(867,174,'c0 + c1 > 10',21,C['purple'][2],True);s.line([(827,310),(974,310)],'purple')
s.t(977,143,'输出 P',23,C['blue'][2],True)
s.box(977,200,271,86,'位置 0 ← reader 0',['x=4    p=east'],'blue',20);s.box(977,342,271,86,'位置 1 ← reader 2',['x=9    p=east'],'blue',20)
s.t(493,505,'selectedIndices = [0, 2]',20,C['purple'][2],True)
s.line([(32,537),(1248,537)],arrow=False,color='#dce3ea');s.t(32,563,'rowsScanned = 5',20);s.t(493,563,'reader 行数 = 3',20);s.t(977,563,'remaining 后 = 2',20);s.t(32,592,'最终行对应文件 [0, 3]；c1 不交给下游，c2 从未进入 reader 的输出 RowVector。',20);s.save('batch')

s=SVG('类型与 channel：输出前缀固定，辅助列只向尾部追加','schemaType 描述表列；dataType 描述交付值。extraction 可让请求类型与产生类型不同。',634)
labels=[('计划 / P', ['keys : ARRAY','p : VARCHAR'], 'blue'),('构造后 R',['m : MAP','p : VARCHAR','c1 : BIGINT'],'purple'),('split 扩展 R',['m : MAP','p : VARCHAR','c1 : BIGINT','key : BIGINT'],'purple'),('对应 G',['m : ARRAY','p : VARCHAR','c1 : BIGINT','key : BIGINT'],'green')]
for i,(lab,cells,key) in enumerate(labels):
 y=154+i*95;s.t(32,y+32,lab,22,C[key][2],True)
 for j,c in enumerate(cells):s.box(244+j*245,y,224,60,c,key=key,size=20)
s.t(256,125,'channel 0',18);s.t(501,125,'channel 1',18);s.t(746,125,'channel 2',18);s.t(991,125,'channel 3',18)
s.line([(244,530),(713,530)],'blue',False,width=2.2);s.t(478,561,'前 N=2 列：最终投影',21,C['blue'][2],True,'middle')
s.line([(734,530),(1203,530)],'warm',False,width=2.2);s.t(968,561,'隐藏依赖：remaining / equality',20,C['warm'][2],True,'middle')
s.t(32,608,'keys → handle.name=m；最终 RowVector 恢复 keys。G 非空时用于分配，不能用 R 的 MAP 代替 ARRAY。',20)
s.save('types')

s=SVG('构造阶段：原始过滤输入如何分成三条执行路径','保留 TableHandle 输入；派生 Filter map、残余 TypedExpr、ExprSet 和与树绑定的 MetadataFilter。',699)
s.box(32,135,344,113,'TableHandle 原始输入',['subfieldFilters','remaining TypedExpr'],'blue')
s.box(446,135,398,113,'提取可下推过滤条件',['extractFiltersFromRemainingFilter','副本中合并 · 原始输入不变'],'purple')
s.line([(376,191),(446,191)],'purple');s.box(917,135,331,113,'采样模式（可选）',['sampleRate','RandomSkipTracker'],'warm');s.line([(844,191),(917,191)],'warm')
s.box(32,314,344,113,'派生 filters_',['原始 + 可提取条件','Filter payload 可共享'],'purple')
s.box(470,314,374,113,'残余 TypedExpr',['无法逐列下推的表达式','如 c0=1 OR c1=2'],'blue')
s.line([(508,248),(508,280),(204,280),(204,314)],'purple');s.line([(722,248),(722,314)],'blue')
s.box(32,499,344,110,'ScanSpec',['节点 / channel / constants','安装普通逐行 Filter'],'green')
s.box(470,499,374,110,'ExprSet / compiled Expr',['distinctFields → 辅助列','extractSubfields → 读取需求'],'blue')
s.box(910,499,338,110,'MetadataFilter',['leaf 指针登记到 ScanSpec','AND / OR 统计排除位图'],'purple')
s.line([(204,427),(204,499)],'green');s.line([(657,427),(657,499)],'blue');s.line([(844,366),(1079,366),(1079,499)],'purple')
s.line([(470,556),(376,556)],'blue');s.line([(1079,609),(1079,635),(204,635),(204,609)],'purple',dashed=True)
s.t(32,675,'树被重建时，旧 MetadataFilter 非空 ≠ leaf 已登记到新树；metadata 与逐行 ExprSet 是独立保证。',20)
s.save('filter-build')

s=SVG('Bucket conversion：4 桶扩到 8 桶','历史文件不重写；按当前表桶号重新选择行。桶参数变化与读取需求变化是两件事。',631)
s.t(32,147,'旧分区桶 1：h % 4 = 1',23,C['blue'][2],True)
for j,h in enumerate([1,5,9,13]):
 s.box(32+j*150,174,128,84,f'h={h}',[f'新桶 {h%8}'],'blue')
s.line([(96,258),(96,302)],'green')
s.line([(246,258),(246,277),(476,277),(476,302)],'purple')
s.line([(396,258),(396,289),(172,289),(172,302)],'green')
s.line([(546,258),(546,302)],'purple')
s.box(32,302,280,86,'目标 tableBucketNumber=1',['保留 h=1、9'],'green',20);s.box(336,302,280,86,'目标 tableBucketNumber=5',['保留 h=5、13'],'purple',20)
s.t(690,147,'同一 DataSource 的连续 split',23,C['green'][2],True)
s.box(690,174,558,100,'split A：首次需要隐藏 k',['R=[c0] → [c0,k]；取消 k 的子字段裁剪','读取需求变化 → 重建 ScanSpec'],'green',22)
s.box(690,315,558,100,'split B：同样读取 k，目标桶变化',['R=[c0,k] 保留；嵌套节点 / 反馈继续保留','读取需求不变 → 复用 ScanSpec'],'purple',22);s.line([(969,274),(969,315)],'purple')
s.box(32,457,1216,76,'每个带 conversion 的 split：创建新的 HivePartitionFunction',['DWIO reader Filter → 完整 bucket key 的 Hive hash → 桶行筛选 → remaining expression'],'warm',22)
s.note(581,['查询即使不投影 k，也要把它读到内部 RowVector；只读 ROW 的部分字段可能改变 hash。']);s.save('bucket')

s=SVG('行标识：物理位置、Hive 复合 ID 与 Iceberg lineage','row index 是文件绝对行位置；同名 “row ID” 并不意味着相同类型或相同稳定性。',705)
s.t(32,146,'Hive kRowId',24,C['blue'][2],True)
for i,(name,typ,src) in enumerate([('row_number','BIGINT','DWIO：文件起点 + selected row'),('row_group_id','VARCHAR','split.getFileName()，不是 Parquet 组号'),('metadata_version','BIGINT','rowIdProperties.metadataVersion'),('partition_id','BIGINT','rowIdProperties.partitionId'),('table_guid','VARCHAR','rowIdProperties.tableGuid')]):
 y=170+i*69;s.rect(32,y,1216,59,'blue' if i==0 else 'warm');s.t(48,y+37,name,21,'#1f2d3d',True);s.t(315,y+37,typ,20,C['blue'][2],mono=True);s.t(498,y+37,src,20)
s.t(32,560,'Iceberg MERGE 定位',22,C['purple'][2],True);s.t(396,560,'ROW(file_path, row_position, spec_id, partition_data)',20,'#1f2d3d')
s.t(32,607,'Iceberg V3 lineage',22,C['green'][2],True);s.t(396,607,'_row_id : BIGINT；缺失值用 firstRowId + 文件位置继承',20,'#1f2d3d')
s.note(671,['已有非 null lineage 值保留。物理文件定位不自动成为跨文件重写稳定的逻辑主键。']);s.save('row-id')

s=SVG('Iceberg 的过滤分路：位置先行，值比较在后','箭头表示同一路径内的处理顺序；changelog 不允许携带 deleteFiles，不能与普通扫描合并画。',736)
s.t(32,145,'普通 Iceberg 数据扫描',24,C['blue'][2],True)
s.box(32,173,340,113,'文件绝对行范围',['nextRowNumber · nextReadSize','metadata 跳读已反映到位置'],'blue')
s.box(450,173,344,113,'position delete / Puffin DV',['填相对本批 deletedRows 位图','Mutation 交给 DWIO'],'purple')
s.box(872,173,376,113,'DWIO reader',['Mutation → 列 Filter','输出所需列 + equality key'],'green')
s.line([(372,229),(450,229)],'purple');s.line([(794,229),(872,229)],'green')
s.box(872,355,376,113,'填充 / 转换',['geometry · lineage · target ID','填完后移除临时行号列'],'blue');s.line([(1060,286),(1060,355)],'blue')
s.box(450,355,344,113,'读后选择',['post-read Filter → equality delete','共用位图；字典包装一次'],'purple');s.line([(872,411),(794,411)],'purple')
s.box(32,355,340,113,'FileDataSource',['remaining expression','输出前缀 / postProcessor / 改名'],'green');s.line([(450,411),(372,411)],'green')
s.line([(32,509),(1248,509)],arrow=False,color='#dce3ea');s.t(32,548,'Changelog 独立路径',24,C['warm'][2],True)
for x,w,title,lines,key in [(32,340,'split 常量过滤',['operation / ordinal / snapshotid'],'warm'),(450,344,'基础表读取',['dataScanSpec · 无 deleteFiles'],'blue'),(872,376,'包装 changelog RowVector',['rowdata + 常量 → remaining'],'green')]:s.box(x,578,w,87,title,lines,key)
s.line([(372,621),(450,621)],'blue');s.line([(794,621),(872,621)],'green');s.t(32,707,'Equality delete 要读取实际 key；Position delete / DV 依据文件行位置，不依赖压缩后的 batch 下标。',20)
s.save('iceberg')

s=SVG('动态过滤：Driver 合并，DataSource 替换节点引用','shared PushdownFilters 保存累计条件；普通 FileDataSource 没有额外 dynamicFilters_ 缓存。',640)
s.box(32,151,337,100,'Join 等生产者',['输出 channel','产生本轮动态 Filter'],'purple')
s.box(443,151,360,100,'Driver::pushdownFilters',['沿 identityProjection 回溯','映射到 TableScan channel'],'blue')
s.box(876,151,372,100,'shared PushdownFilters',['静态根条件 + 动态条件合并','记录 dynamicFilteredColumns'],'purple')
s.line([(369,201),(443,201)],'blue');s.line([(803,201),(876,201)],'purple')
s.box(876,331,372,100,'TableScan / DataSource',['按 output channel 找 ScanSpec','setFilter：替换节点引用'],'green');s.line([(1062,251),(1062,331)],'green')
s.box(443,331,360,100,'ScanSpec 缓存',['递归 resetCachedValues(true)','清 hasFilter，允许重排'],'blue');s.line([(876,381),(803,381)],'blue')
s.box(32,331,337,100,'RowReader / ColumnReader',['resetFilterCaches','旧字典过滤结果 → Unknown'],'green');s.line([(443,381),(369,381)],'green')
s.t(32,488,'下一个普通 split',22,C['green'][2],True);s.t(320,488,'复用同一 ScanSpec；常量按新 split 重新测试。',21)
s.t(32,536,'preload / takeover',22,C['warm'][2],True);s.t(320,536,'donor 是另一棵树；晚到条件受 moveAdaptationFrom 条件约束。',21)
s.note(598,['prepare 后的非 null 常量不保证重新测试；动态过滤剪枝与下游 join 的精确条件分开判断。']);s.save('dynamic')

s=SVG('LazyVector：行映射与 reader 世代共同约束加载','buffer 可共享不代表未加载值可以跨 reader 前进；最终选中行还要映射到原文件位置。',650)
s.t(32,146,'当前批：generation = 7',24,C['blue'][2],True)
s.box(32,176,322,115,'reader outputRows',['[0, 2, 3]：文件批内位置','ColumnReader 仍在本批'],'blue')
s.box(453,176,356,115,'remaining selection',['[0, 2]：reader 输出下标','dictionary 包装 LazyVector'],'purple')
s.box(908,176,340,115,'ColumnLoader 加载',['outputRows[[0,2]] → [0,3]','按实际文件位置读值'],'green')
s.line([(354,233),(453,233)],'purple');s.line([(809,233),(908,233)],'green')
s.box(32,359,531,116,'正确：本世代内完成必要加载',['检查 version == structReader.numReads()','已物化结果跨批保留，由引用计数保护'],'green')
s.box(653,359,595,116,'不成立：保留 lazy 指针，先推进下一批',['generation 7 → 8 后再加载：版本检查失败','销毁 reader 后更不能依靠 LazyVector 延长其生命'],'warm')
s.line([(1078,291),(1078,332),(298,332),(298,359)],'green')
s.note(543,['eager-load 处理投影列与 conditional 选择的冲突；只需少量行时，lazy 可减少 payload 解码。','remaining dictionary、Iceberg delete dictionary、postProcessor 还需共同遵守共享向量的可写性。']);s.save('lazy')

s=SVG('Preload → takeover：准备与消费分开，接管关联状态','实线为同步调用，虚线为返回 / 完成；生命线表示参与者。AsyncSource.move 可同步等待。',842)
xs=[150,466,793,1125];titles=['TableScan / receiver','AsyncSource','preload executor','donor DataSource']
for x,title in zip(xs,titles):
 s.box(x-130,128,260,56,title,key='blue' if x==150 else 'purple',size=20)
 s.line([(x,184),(x,714)],'gray',False,True,2.3)
def msg(x1,x2,y,label,key='blue',dash=False):
 s.line([(x1,y),(x2,y)],key,True,dash);s.t((x1+x2)/2,y-10,label,19,C[key][2],anchor='middle')
msg(150,466,226,'注册下一 split 的 maker')
msg(793,466,286,'prepare()','purple')
msg(793,1125,347,'创建 DS / 编译 / addSplit','purple')
msg(1125,793,411,'Reader / RowReader 准备完成','green',True)
msg(793,466,470,'发布 donor；满足 promise','green',True)
msg(150,466,532,'当前 split 结束：move()')
msg(466,150,588,'返回 donor，或重抛准备异常','green',True)
s.box(42,630,1176,75,'receiver.setFromDataSource(donor)',['移 split / R / G / ScanSpec / metadata / reader；保留 receiver ExprSet；重绑 ctx，合并 I/O 统计'],'warm',22)
s.note(758,['T0 donor 准备后、T1 receiver 收到的新动态条件：T2 接管只按 moveAdaptationFrom 的规则迁移。','普通 addSplit 不重复编译 residual；preload 新建一份 DataSource，所以会额外编译与准备。']);s.save('takeover')
print('SVG diagrams:',len(list(D.glob('*.svg'))))
