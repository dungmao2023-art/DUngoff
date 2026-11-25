import os, sys, time, subprocess, json
# Kiểm thử cục bộ (Linux) cho dung_api.py: cần soffice + python3-uno. Chạy: python3 -I test_dung_api_local.py
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uno
from com.sun.star.beans import PropertyValue
import dung_api as D
p = subprocess.Popen(['soffice','--headless','--invisible','--norestore','-env:UserInstallation=file:///tmp/apitest/prof',
  '--accept=socket,host=localhost,port=2099;urp;'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
ctx=None
for i in range(60):
    try:
        lc = uno.getComponentContext()
        r = lc.ServiceManager.createInstanceWithContext('com.sun.star.bridge.UnoUrlResolver', lc)
        ctx = r.resolve('uno:socket,host=localhost,port=2099;urp;StarOffice.ComponentContext'); break
    except Exception: time.sleep(1)
smgr = ctx.ServiceManager
desk = smgr.createInstanceWithContext('com.sun.star.frame.Desktop', ctx)
def new(kind):
    h = PropertyValue(); h.Name='Hidden'; h.Value=True
    return desk.loadComponentFromURL('private:factory/'+kind,'_blank',0,(h,))
fails=0
def chk(name, res, ok=True):
    global fails
    good = res['ok']==ok
    print(('PASS' if good else 'FAIL'), name, res['message'][:90])
    if not good: fails+=1
try:
    w = new('swriter')
    w.Text.setString('Tieu de\nNoi dung')
    chk('ping', D.run_call(w,{'nonce':'n','fn':'ping','args':{}}))
    chk('margins', D.run_call(w,{'fn':'page_margins','args':{'left_mm':30,'right_mm':15,'top_mm':20,'bottom_mm':20}}))
    r = D.run_call(w,{'fn':'read_page_margins','args':{}}); chk('read margins', r)
    st = r['data']['styles'][0]; assert abs(st['left_mm']-30)<0.02, st
    chk('page_setup', D.run_call(w,{'fn':'page_setup','args':{'paper':'A4','orientation':'portrait'}}))
    chk('header_footer', D.run_call(w,{'fn':'header_footer','args':{'footer':True,'footer_page_number':True,'header':True,'header_text':'Dau trang'}}))
    chk('paragraph', D.run_call(w,{'fn':'paragraph_format','args':{'align':'justify','line_spacing_percent':150,'first_line_mm':10}}))
    chk('toc', D.run_call(w,{'fn':'toc_insert','args':{}}))
    chk('toc_update', D.run_call(w,{'fn':'toc_update','args':{}}))
    chk('outline', D.run_call(w,{'fn':'outline_numbering','args':{'levels':3}}))
    chk('doc_props', D.run_call(w,{'fn':'doc_properties','args':{'title':'T','keywords':['a','b']}}))
    chk('bad fn', D.run_call(w,{'fn':'os.system','args':{}}), False)
    chk('extra arg', D.run_call(w,{'fn':'page_margins','args':{'left_mm':1,'right_mm':1,'top_mm':1,'bottom_mm':1,'x':1}}), False)
    chk('calc fn on writer', D.run_call(w,{'fn':'sort_range','args':{}}), False)
    chk('expect_url mismatch', D.run_call(w,{'fn':'ping','args':{},'expect_url':'file:///nope.docx'}), False)
    w.close(True)
    c = new('scalc'); sh = c.Sheets.getByIndex(0)
    for i,v in enumerate(['c','a','b']): sh.getCellByPosition(0,i).setString(v)
    sh.getCellByPosition(1,0).setValue(5)
    sh.getCellByPosition(2,0).setString('007,x')
    chk('cell_format', D.run_call(c,{'fn':'cell_format','args':{'range':'A1:B3','number_format':'vnd','bold':True,'bg_color':'#FFFF00','border':'all','halign':'center'}}))
    chk('validation list', D.run_call(c,{'fn':'data_validation','args':{'range':'D1:D5','kind':'list','items':['Thu','Chi']}}))
    chk('validation num', D.run_call(c,{'fn':'data_validation','args':{'range':'E1:E5','kind':'whole','min':1,'max':10}}))
    chk('cond fmt', D.run_call(c,{'fn':'conditional_format','args':{'range':'B1:B3','rule':'greater','value':3,'bg_color':'#FF0000'}}))
    chk('sort', D.run_call(c,{'fn':'sort_range','args':{'range':'A1:A3','keys':[{'column':0,'ascending':True}],'has_header':False}}))
    print('sorted:', [sh.getCellByPosition(0,i).getString() for i in range(3)])
    chk('text_to_columns', D.run_call(c,{'fn':'text_to_columns','args':{'range':'C1:C1','delimiter':'comma'}}))
    print('ttc:', sh.getCellByPosition(2,0).getString(), sh.getCellByPosition(3,0).getString())
    chk('protect', D.run_call(c,{'fn':'protect_sheet','args':{'protect':True,'password':'x'}}))
    chk('unprotect wrong', D.run_call(c,{'fn':'protect_sheet','args':{'protect':False,'password':'bad'}}), False)
    chk('unprotect', D.run_call(c,{'fn':'protect_sheet','args':{'protect':False,'password':'x'}}))
    chk('bad range', D.run_call(c,{'fn':'cell_format','args':{'range':'A1;B2','bold':True}}), False)
    c.close(True)
finally:
    try: desk.terminate()
    except Exception: pass
    p.terminate()
print('FAILS', fails)
