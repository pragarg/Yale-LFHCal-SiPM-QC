#this file turns the json files into a dataset that can be used for training and testing

import json, base64, io, os
import numpy as np
import torch
from torch.utils.data import Dataset
from PIL import Image
import torchvision.transforms.functional as TF


def load_records(json_paths):
    """Read label JSON files and deduplicate records by chip key."""
    seen = {}
    for p in json_paths:
        for r in json.load(open(p))["images"]:
            seen[r.get("key") or r["filename"]] = r
    return list(seen.values())


class SiPMDataset(Dataset):
    def __init__(self, records, image_dir, size=512, augment=None):
        self.records = records
        self.image_dir = image_dir
        self.size = size
        self.augment = augment   #albumentations Compose from the training notebook, or None

    def __len__(self):
        return len(self.records)

    @staticmethod
    
    def _decode_mask(data_url, h, w):
        if not data_url:
            return np.zeros((h, w), dtype=np.uint8)
        b64 = data_url.split(",", 1)[1]
        m = Image.open(io.BytesIO(base64.b64decode(b64)))
        #the labeler paints into the alpha channel, so read alpha not brightness. convert("L") throws alpha away and counts the faded edge of every brush stroke as solid paint
        m = m.getchannel("A") if m.mode in ("RGBA", "LA") else m.convert("L")
        m = np.array(m)
        if m.shape != (h, w):
            m = np.array(Image.fromarray(m).resize((w, h), Image.NEAREST))
        return (m > 8).astype(np.uint8)

    @staticmethod
    #pads the image so that it is square and doesn't lose resolution during convolution
    def _pad_square(img, fill):
        w, h = img.size
        s = max(w, h)
        out = Image.new(img.mode, (s, s), fill)
        out.paste(img, ((s - w) // 2, (s - h) // 2))
        return out

    # makes a map of the pixels in the image, where 0 is background, 1 is damage, and 2 is artifact
    def _build_label(self, r):
        h, w = r["height"], r["width"]
        dmg = self._decode_mask(r.get("damageMask"), h, w)
        fls = self._decode_mask(r.get("falseMask"), h, w)
        lab = np.zeros((h, w), dtype=np.uint8)     # background
        lab[fls == 1] = 2                          # artifact
        lab[dmg == 1] = 1                          # damage overrides artifact
        return lab

    def __getitem__(self, i):
        r = self.records[i]
        # Records normally store paths as "<tray>/<filename>".
        rel = r.get("key") or r["filename"]
        img = Image.open(os.path.join(self.image_dir, rel)).convert("RGB")
        lab = Image.fromarray(self._build_label(r))

        img = self._pad_square(img, 0).resize((self.size, self.size), Image.BILINEAR)
        lab = self._pad_square(lab, 0).resize((self.size, self.size), Image.NEAREST)

        #augmentation has to hit the image and the mask together or the labels stop lining up
        img = np.array(img)
        lab = np.array(lab)
        if self.augment is not None:
            out = self.augment(image=img, mask=lab)
            img, lab = out["image"], out["mask"]

        x = TF.normalize(TF.to_tensor(img),
                         [0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
        y = torch.from_numpy(np.ascontiguousarray(lab)).long()
        has_damage = int((lab == 1).any())
        return x, y, has_damage
