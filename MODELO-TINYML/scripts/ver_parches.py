import numpy as np, sys
from pathlib import Path
from PIL import Image
D=Path(__file__).resolve().parent.parent/"datos"/"clasificacion-96"
OUT=Path("/private/tmp/claude-501/-Users-mecatronica-Documents-edgeForest/9488eb04-846f-4992-b65e-f105689776f2/scratchpad")
X=np.load(D/"X_train.npy"); y=np.load(D/"y_train.npy")
rng=np.random.default_rng(3)
for cls,nombre in [(1,"humo"),(0,"nohumo")]:
    idx=rng.choice(np.where(y==cls)[0],24,replace=False)
    c=Image.new("RGB",(8*100,3*100),(15,15,15))
    for i,j in enumerate(idx):
        im=Image.fromarray(X[j]).resize((96,96))
        c.paste(im,((i%8)*100+2,(i//8)*100+2))
    p=OUT/f"parches_{nombre}.jpg"; c.save(p,quality=88); print(p)
