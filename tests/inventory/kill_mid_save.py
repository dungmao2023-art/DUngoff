# Giết tiến trình giữa lúc lưu, nhiều lần, rồi kiểm tra: tệp đích còn hợp lệ hay không? tệp tạm còn sót?
import sys, os, subprocess, time, signal, glob, zipfile, shutil
exe, program, src, outdir = sys.argv[1:5]
runs = int(sys.argv[5]) if len(sys.argv) > 5 else 4
target_dir = os.path.join(outdir, "dich")
shutil.rmtree(outdir, ignore_errors=True)
os.makedirs(target_dir)
target = os.path.join(target_dir, "ket-qua" + os.path.splitext(src)[1])
def valid(p):
    try:
        with zipfile.ZipFile(p) as z:
            return z.testzip() is None and "[Content_Types].xml" in z.namelist()
    except Exception:
        return False
orig_before = open(src, "rb").read()
corrupt = leftover_total = killed_during_tmp = 0
for i in range(runs):
    proc = subprocess.Popen([exe, "victim", program, src, target, os.path.join(outdir, "v%d" % i)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    t0 = time.time(); saw = False
    while time.time() - t0 < 240 and proc.poll() is None:
        if glob.glob(os.path.join(target_dir, "~dung-*")):
            saw = True
            time.sleep(0.05 * (i + 1))   # giết ở các thời điểm khác nhau trong lúc ghi
            break
        time.sleep(0.01)
    if proc.poll() is None:
        proc.kill()
    proc.wait()
    if saw: killed_during_tmp += 1
    tmps = glob.glob(os.path.join(target_dir, "~dung-*"))
    leftover_total += len(tmps)
    exists = os.path.exists(target)
    ok = valid(target) if exists else None
    print("INV kill%d_thay_tep_tam=%d tep_dich_ton_tai=%s tep_dich_hop_le=%s tep_tam_con_sot=%d" % (i, saw, exists, ok, len(tmps)))
    if exists and not ok: corrupt += 1
    for t in tmps:
        try: os.remove(t)
        except Exception: pass
print("INV kill_tong_so_lan=%d" % runs)
print("INV kill_giet_dung_luc_ghi_tep_tam=%d" % killed_during_tmp)
print("INV kill_tep_dich_hong=%d" % corrupt)
print("INV kill_tep_tam_con_sot_tong=%d" % leftover_total)
print("INV kill_tep_goc_nguyen_byte=%d" % (open(src, "rb").read() == orig_before))
