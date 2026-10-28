import numpy as np
import pytest
from realtime.silhouette import mask_to_blobs


def test_empty_mask_has_no_blobs():
    assert mask_to_blobs(np.zeros((100, 100), dtype=np.uint8)) == []


def test_blob_is_normalized_and_noise_filtered():
    mask = np.zeros((480, 640), dtype=np.uint8)
    mask[100:400, 160:480] = 1
    mask[20:22, 20:22] = 1
    blobs = mask_to_blobs(mask)
    assert len(blobs) == 1
    blob = blobs[0]
    assert blob['cx'] == pytest.approx(0.5, abs=0.01)
    assert blob['cy'] == pytest.approx(250/480, abs=0.01)
    assert blob['area'] > 0.29
    assert len(blob['contour']) == 4
    assert all(0 <= p['x'] <= 1 and 0 <= p['y'] <= 1 for p in blob['contour'])


def test_disjoint_blobs_and_holes_use_external_boundaries():
    mask = np.zeros((100, 100), dtype=np.uint8)
    mask[5:45, 5:45] = 1
    mask[15:25, 15:25] = 0
    mask[60:95, 60:95] = 1
    assert len(mask_to_blobs(mask)) == 2
    assert len(mask_to_blobs(mask, max_blobs=1)) == 1


def test_irregular_contour_has_bounded_payload():
    mask = np.zeros((300, 300), dtype=np.uint8)
    mask[100:250, 10:290] = 1
    for x in range(10, 290, 6):
        mask[50:100, x:x+3] = 1
    blobs = mask_to_blobs(mask, max_points=32)
    assert blobs
    assert 3 <= len(blobs[0]['contour']) <= 32


def test_full_frame_is_valid_but_not_identity_or_depth():
    blobs = mask_to_blobs(np.ones((100, 100), dtype=np.uint8))
    assert len(blobs) == 1
    assert blobs[0]['label'] == 'person'
    assert blobs[0]['w'] == 1


@pytest.mark.parametrize('mask', [np.zeros((0, 0)), np.zeros((5, 5, 3))])
def test_invalid_masks_rejected(mask):
    with pytest.raises(ValueError):
        mask_to_blobs(mask)
