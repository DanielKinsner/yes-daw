"""Reuse reviewer's arithmetic checks only; no author or target import."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
prior=ROOT/'build-ci/20261106-musical-funded-legacy-adapter-r4-numeric-independent-review-r4/evidence/check_packet.py'
text=prior.read_text(encoding='utf-8')
text=text[:text.index('# Concrete independent source finding;')]
text=text.replace('20261105-musical-funded-legacy-adapter-r4-census-r4','20261106-musical-funded-legacy-adapter-r4-census-r5')
for old,new in [
 ('9A587302A9093829EE799D8B7C3E5F921B2C86D834999382909B7F229843E8B2','EB0EFB963D5B13587DBA9B0A3B1B7C390BB153781D9DFC3510209659690BEC5A'),
 ('BDBA8D8E2874BF7BD404108B575719EB0BF956C987E217F5A3DAD9E9BBE41E6E','E50F8ACCCBD222F78992FA0797C474B863B3122653D8C77C8C8765A5D8E7D33E'),
 ('C1AE759090DBC1002BC35F7E1A5B26ED4A6BA5A49FA712D43E4D6B645DD1611A','D17908488D90F537B9235F0D4C10924B66D2BD0FC4B647C91E44F8B78A9AD554')]:
    assert text.count(old)==1
    text=text.replace(old,new)
(Path(__file__).parent/'check_packet.py').write_text(text,encoding='utf-8',newline='\n')
print('New independent R5 checker prefix written; no subject changed/imported.')
