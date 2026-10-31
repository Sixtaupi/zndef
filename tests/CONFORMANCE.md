# Vérification de conformité et traçabilité

Ce dossier vérifie les règles du codec à partir des PDF présents dans `doc`.
Les identifiants de tests désignent exactement les deux arguments
de leur déclaration `ZTEST`. Les références ne sont validées que si le test existe
et si Twister le rapporte exécuté avec succès. Le nombre de tests, la présence
d'un appel d'API et la couverture du code sont des indicateurs complémentaires ;
aucun ne remplace une exigence normative et son oracle indépendant.

## Documents examinés

| Document local | Édition | Portée |
|---|---|---|
| `doc/NFCForum-TS-NDEF_1.0.pdf` | NDEF 1.0 | Messages, records, champs, flags, TNF, chunking, traitement des erreurs |
| `doc/NFCForum-TS-RTD_1.0.pdf` | RTD 1.0 | Noms bien connus et externes, portée, syntaxe, comparaison, encodage binaire |
| `doc/NFCForum-TS-RTD_Text_1.0.pdf` | Text RTD 1.0 | Statut, langue, UTF-8/UTF-16, ordre des octets, sauts de ligne |
| `doc/NFCForum-TS-RTD_URI_1.0.pdf` | URI RTD 1.0 | Table des préfixes, RFU, contrôles et UTF-8, exemples |
| `doc/NFCForum-SmartPoster_RTD_1.0.pdf` | Smart Poster RTD 1.0 | Message imbriqué, URI, titres, action, taille, type, icônes |

`NFC-Data-Exchange-Format-NDEF.pdf` est une copie octet pour octet de
`NFCForum-TS-NDEF_1.0.pdf`, et ne représente pas une sixième spécification.
Les empreintes SHA-256 exactes des PDF, sources, headers et tests sont intégrées
au rapport d'évidence. Les RFC citées par les PDF ne sont pas elles-mêmes
fournies : les tests lexicaux ne valent pas une validation exhaustive de ces RFC.

## NDEF et RTD

| Exigence | Référence exacte | Tests | Résultat attendu / portée | État |
|---|---|---|---|---|
| Un message comporte au moins un record ; MB au début, ME à la fin | §2.3.1, p.8 ; §3.2.1–2 | `ndef_spec.test_all_header_flag_combinations` ; `ndef_spec.test_tnf_wire_matrix` | Sur les 32 combinaisons de cinq flags, seuls MB=ME=1, CF=0 forment un message mono-record complet ; SR et IL ne changent pas cette décision | Automatisé ; verdict dans le rapport Twister |
| Les messages ne se chevauchent pas ; les flags ne servent pas à imbriquer | §2.3.1, p.8 | `ndef_spec.test_chunk_forbidden_message_flags` ; `ndef_spec.test_nested_message_wire_and_opaque_payload` | MB additionnel et ME dans chunks initiaux/intermédiaires rejetés ; imbrication dans PAYLOAD acceptée | Automatisé ; verdict dans le rapport Twister |
| Le message doit être reçu entier | §2.2, p.7, ligne 388 ; §2.5, p.11, ligne 542 | `ndef_spec.test_chunk_writer_wire_and_logical_view` ; `ndef_spec.test_normal_short_field_order` | Toutes les troncatures du record normal et du message chunké sont rejetées ; le transport intégral demeure une responsabilité d'intégration | Partiel : codec testé, transport hors portée |
| Chunks initial + zéro ou plusieurs intermédiaires + terminal | §2.3.3, p.8–9 | `ndef_spec.test_chunk_writer_wire_and_logical_view` ; `ndef_spec.test_zero_length_chunks_and_il_restriction` | Vecteurs fixes à trois chunks et deux chunks ; charges initiale et terminale de longueur zéro autorisées | Automatisé ; verdict dans le rapport Twister |
| CF initial/intermédiaire=1, CF terminal=0 ; aucun ME avant terminal | §2.3.3 ; §3.2.3 | `ndef_spec.test_chunk_forbidden_message_flags` ; `ndef_spec.test_chunk_writer_wire_and_logical_view` | Neuf mutations indépendantes de flags/TNF refusées ; encoder produit B9/36/56 exactement | Automatisé ; verdict dans le rapport Twister |
| TYPE et ID de la charge logique dans le chunk initial ; continuations TNF=06, TYPE_LENGTH=IL=0 | §2.3.3 ; §3.2.6 et §3.2.11 | `ndef_spec.test_chunk_writer_wire_and_logical_view` ; `ndef_spec.test_tnf_forbidden_fields` ; `ndef_spec.test_zero_length_chunks_and_il_restriction` | TYPE/ID présents une fois ; IL=1 est refusé dans continuation même avec ID_LENGTH=0 ; TNF inchangé hors séquence refusé | Automatisé ; verdict dans le rapport Twister |
| Chaque longueur physique de chunk ne décrit que ce chunk | §2.4.1 ; §2.3.3 | `ndef_spec.test_chunk_writer_wire_and_logical_view` | `record_decode` voit longueur 1 et premier record physique de 7 octets ; itérateur logique voit longueur totale 3 et trois chunks | Automatisé ; verdict dans le rapport Twister |
| Octets numériques multi-octets MSB d'abord | §3.1, p.13 ; §3.2.9 | `ndef_spec.test_normal_short_field_order` ; `ndef_spec.test_writer_payload_size_boundaries` ; `ndef_spec.test_extreme_declared_lengths_are_bounded` | Longueurs 256=00 00 01 00 et 257=00 00 01 01 ; octet fort non nul ne devient pas longueur faible | Automatisé ; verdict dans le rapport Twister |
| Ordre des champs : header, longueurs, TYPE, ID, PAYLOAD | §3.2 figure 3, p.14 | `ndef_spec.test_normal_short_field_order` | Positions exactes TYPE/ID/PAYLOAD comparées à deux vecteurs fixes, versions SR=0 et SR=1 | Automatisé ; verdict dans le rapport Twister |
| Parser accepte normal, short et leur mélange | §3.2.4, p.15 ; §4.3 et §4.5 | `ndef_spec.test_mixed_short_normal_message` ; `ndef_spec.test_all_header_flag_combinations` ; `ndef_spec.test_normal_short_field_order` | Les deux ordres normal/short sont acceptés ; aucune restriction fondée seulement sur SR | Automatisé ; verdict dans le rapport Twister |
| IL commande présence du champ ID_LENGTH ; zéro est permis | §3.2.5 ; §3.2.8 | `ndef_spec.test_normal_short_field_order` ; `ndef_spec.test_all_header_flag_combinations` ; `ndef_spec.test_tnf_forbidden_fields` | ID_LENGTH présent quand IL=1 ; ID absent si longueur zéro, y compris TNF Empty avec IL=1 | Automatisé ; verdict dans le rapport Twister |
| TNF Empty : TYPE_LENGTH, ID_LENGTH, PAYLOAD_LENGTH zéro | §3.2.6, p.16 | `ndef_spec.test_tnf_forbidden_fields` ; `ndef_spec.test_tnf_wire_matrix` | TYPE, ID ou PAYLOAD non vides refusés ; record D0 00 00 accepté | Automatisé ; verdict dans le rapport Twister |
| TNF 1/2/3/4 indique respectivement RTD, MIME, URI absolue, externe | §3.2.6 ; §3.2.10 | `ndef_spec.test_tnf_wire_matrix` ; `rtd_spec.test_binary_encoding_examples` | Vecteurs explicites des huit TNF, RTD=01 et externe=04 ; ceci ne prouve pas la grammaire RFC complète | Partiel : RFC MIME/URI non auditées intégralement |
| Unknown : TYPE_LENGTH zéro, payload opaque disponible | §3.2.6 | `ndef_spec.test_tnf_wire_matrix` ; `ndef_spec.test_tnf_forbidden_fields` ; `ndef_spec.test_nested_message_wire_and_opaque_payload` | TNF05 avec octet FF conservé ; TYPE non vide refusé ; aucun dispatch imposé par NDEF | Automatisé ; verdict dans le rapport Twister |
| Unchanged uniquement dans continuations ; TYPE_LENGTH zéro | §3.2.6 | `ndef_spec.test_tnf_wire_matrix` ; `ndef_spec.test_tnf_forbidden_fields` ; `ndef_spec.test_chunk_forbidden_message_flags` | Lecture physique peut exposer TNF06 ; validation logique le refuse hors séquence | Automatisé ; verdict dans le rapport Twister |
| TNF07 réservé interdit aux générateurs ; fallback Unknown conseillé au parser | §3.2.6, p.17 | `ndef_spec.test_tnf_wire_matrix` | Décodage physique conserve header et expose TNF Unknown ; validateur diagnostic retourne erreur pour TNF réservé | Partiel : restriction générateur couverte par `ndef_writer.test_invalid_desc` |
| Longueurs TYPE et ID sur huit bits, maximum 255 | §3.2.7–8 ; §4.3 | `ndef_spec.test_max_type_and_id_lengths` | TYPE=255 et ID=255 acceptés, octets de longueur FF, positions exactes et troncatures vérifiées | Automatisé ; verdict dans le rapport Twister |
| PAYLOAD_LENGTH=0 permis ; SR va jusqu'à 255, normal au-delà | §2.4.1 ; §3.2.4 ; §3.2.9 | `ndef_spec.test_writer_payload_size_boundaries` ; `ndef_spec.test_zero_length_chunks_and_il_restriction` | Tailles 0/1/254/255/256/257 ; deux chemins d'écriture comparés à préfixes binaires fixés indépendamment | Automatisé ; verdict dans le rapport Twister |
| PAYLOAD normal jusqu'à 2^32−1 ; limites de ressources permises | §3.2.9 ; §4.3 | `ndef_spec.test_extreme_declared_lengths_are_bounded` | Déclaration FFFFFFFF sans données refusée sans lecture hors buffer ; aucune allocation effective de 4 Gio testée | Limite documentée, maximum réel non exécuté |
| Payloads logiques supérieurs à 2^32−1 peuvent utiliser chunks | §3.2.9 ; §4.3 | Aucun nouveau test de 4 Gio | Implémentation limite longueur logique à UINT32_MAX et retourne EOVERFLOW ; rejet pour ressources permis §4.3 | Limite ; ne pas prétendre support de toutes tailles NDEF |
| TYPE doit respecter grammaire TNF | §3.2.10 | `rtd_spec.test_well_known_ascii_alphabet_exhaustive` ; `rtd_spec.test_external_ascii_alphabet_exhaustive` | Alphabets RTD testés exhaustivement ; grammaires RFC2046/RFC3986 complètes à auditer séparément | Partiel |
| ID est URI-reference relative ou absolue, unicité côté générateur | §3.2.11 ; §2.4.3 | `ndef_core.test_id_validate`, `ndef_core.test_validate_id` ; unicité et base hors codec | Le writer ne recherche pas les doublons ; application fournit des IDs uniques et une base pour références relatives | Responsabilité application, non couvert |
| PAYLOAD opaque au NDEF générique | §3.2.12 | `ndef_spec.test_nested_message_wire_and_opaque_payload` | FF accepté comme payload Unknown ; validation explicite du payload imbriqué seulement quand son rôle est connu | Automatisé ; verdict dans le rapport Twister |
| Encodage non-ASCII des URI de TYPE laissé à application | §2.4.2 ; §4.1 | `ndef_core.test_type_validate` | Octets non-ASCII conservés pour URI génériques ; aucune sémantique complète de schéma revendiquée | Partiel |
| Exigence | Référence exacte | Tests | Résultat attendu / portée | État |
|---|---|---|---|---|
| RTD well-known utilise TNF01 et nom compact sans préfixe URN | §2.1, p.5 ; §3.1 | `rtd_spec.test_binary_encoding_examples` | Sms encode exactement D1 03 00 53 6D 73 ; les octets canoniques `urn:nfc:wkt:` ne sont pas ajoutés | Automatisé ; verdict dans le rapport Twister |
| Global commence uppercase ; local commence lowercase ou chiffre | §2.1.1–2 ; chapitre 3 | `rtd_spec.test_well_known_ascii_alphabet_exhaustive` ; `rtd_spec.test_global_local_classification` | Les 256 premiers octets sont classés selon ABNF ; exemples explicites de la spec classés global/local | Automatisé ; verdict dans le rapport Twister |
| Attribution globale réservée NFC Forum | §2.1.1 ; §2.3 | Aucun | La bibliothèque ne connaît pas l'autorité qui attribue le nom fourni | Gouvernance, non testable par codec |
| Ne pas traiter un type local sans contexte | §2.1.2 ; §2.3 | `rtd_spec.test_global_local_classification` | Test de classification uniquement ; l'application décide du contexte. Les accès bruts ne constituent pas un dispatch sémantique | Responsabilité application, couverture partielle |
| Externe utilise TNF04, dns-part + ':' + nom non vide ; omet préfixe URN | §2.2 ; §2.3 ; chapitre 3 | `rtd_spec.test_external_required_parts` ; `rtd_spec.test_binary_encoding_examples` | example.com:f émis sans urn:nfc:ext: ; domaine ou nom vide, colon absent et underscore DNS refusés | Automatisé ; verdict dans le rapport Twister |
| DNS-char et WKT-char définis par ABNF, ASCII seulement | Chapitre 3, p.8 ; annexe A, p.12 | `rtd_spec.test_well_known_ascii_alphabet_exhaustive` ; `rtd_spec.test_external_ascii_alphabet_exhaustive` | 256 valeurs testées à chacune des quatre positions : premier WKT, suite WKT, domaine externe, nom externe ; contrôles/NUL/DEL/non-ASCII refusés | Automatisé ; verdict dans le rapport Twister |
| Aucun percent-encoding dans noms global/local | §3.2 ; §3.4 | `rtd_spec.test_well_known_percent_encoding_forbidden` ; `rtd_spec.test_well_known_ascii_alphabet_exhaustive` | Préfixes A/a/0 et escapes pourtant valides tous refusés | Automatisé ; verdict dans le rapport Twister |
| Percent-encoding externe déconseillé mais autorisé ; UTF-8 avant conversion obligatoire | §3.2 | `rtd_spec.test_external_percent_encoding_utf8` | 11 vecteurs UTF-8 valides et 19 invalides : contrôle %HH, continuations isolées, troncatures, overlong, surrogates, >U+10FFFF | Automatisé ; verdict dans le rapport Twister |
| Well-known équivalent ssi représentation binaire identique | §3.3 ; §3.4 ; annexe B | `rtd_spec.test_well_known_equivalence` | Sms différent de sms ; les quatre exemples Foobar/fooBar/fOoBaR/foobar comparés en matrice 4×4 | Automatisé ; verdict dans le rapport Twister |
| Externe comparé sans égard à casse, caractère par caractère | §3.3 ; §3.4 | `rtd_spec.test_external_equivalence` | Quatre variantes officielles en matrice 4×4, différence réelle refusée, aucun percent-décodage implicite | Automatisé ; verdict dans le rapport Twister |
| Ignorer records à type illégal | §4.1 ; §4.3 | `rtd_spec.test_illegal_type_diagnostic_and_application_filter` | Exemple applicatif ignore '/' puis reconnaît U ; validateur diagnostic rejette et writer refuse création invalide | Partiel : obligation de filtre reste applicative |
| Ignorer types well-known/externes inconnus | §4.2 ; §4.3, ligne 486 | `rtd_spec.test_unknown_types_do_not_block_known_record_lookup` | Recherche d'un record U réussit après Z et d:x inconnus ; aucune sémantique inconnue imposée | Partiel : ignore applicatif, pas certification de chaque consommateur |

## Text et URI

| Clause exacte | Exigence / portée | Tests | Politique et résultat attendu |
|---|---|---|---|
| §2.1 ; §3.2.1 ; annexe A, table 4 | Record global bien connu `T` (0x54), un record Text seul est permis, statut/langue/texte, exemple « Hello, world! » | `ndef_spec_text.test_appendix_a_canonical_octets` | Décode le vecteur NDEF constant, parse, vérifie langue et texte ; payload_encode et writer doivent produire les octets exacts. MB/ME ajoutés pour le message à un record, l'exemple laissant cet octet non spécifié. |
| §3.2.1, tables 2 et 3 | Statut : bit 7 UTF-8/UTF-16 ; bit 6 RFU ignoré en réception et écrit à zéro | `ndef_spec_text.test_status_rfu_is_ignored_and_never_written` | Lecture des statuts 0x42 et 0xC2 ; génération attendue 0x02 et 0x82. |
| §3.2.1, table 3 ; §3.3 | Longueur de langue dans les six bits faibles, langue obligatoire, US-ASCII, syntaxe selon RFC 3066 | `ndef_spec_text.test_language_syntax_in_both_directions` ; `ndef_spec_text.test_language_length_and_payload_bounds` | Syntaxe ABNF commune en encode/decode ; valide `en`, `en-US`, `fr-CA`, `i-klingon`, `x-private`, `de-1996`, `zh-Hant-TW`. Rejette langue vide, non ASCII, contrôles, NUL, `_`, point, espaces, sous-tag vide ou >8, premier sous-tag numérique. Limite du champ 63 contre 64 octets. **Pas de validation du registre IANA ou de validité sémantique exhaustive RFC 3066.** |
| §3.2.1, table 2 et formule p.5 | Texte = payload moins statut et langue ; longueur en octets ; texte vide autorisé par le framing | `ndef_spec_text.test_language_length_and_payload_bounds` ; `ndef_spec_text.test_utf8_scalar_boundaries_and_octet_length` | En-tête/langue tronqués rejetés ; texte vide accepté ; comptage explicite d'octets multioctets et d'un NUL incorporé. |
| §3.2.1, tables 2 et 3 ; référence [UNICODE] | UTF-8 valide pour le statut 0 | `ndef_spec_text.test_utf8_scalar_boundaries_and_octet_length` ; `ndef_spec_text.test_invalid_utf8_is_rejected` | Bornes 1/2/3/4 octets ; surlongs, continuations isolées, troncatures, surrogates UTF-8, >U+10FFFF et leaders interdits rejetés en decode/encode/conversion. N'attribue pas les caractères au répertoire Unicode 4.0.1. |
| §3.2.1, tables 2 et 3 ; §3.4 | UTF-16 valide ; BOM toléré ; sans BOM big endian ; BOM omissible en génération | `ndef_spec_text.test_utf16_bom_endianness_and_surrogate_boundaries` ; `ndef_spec_text.test_invalid_utf16_is_rejected_before_conversion` | Vecteurs BE sans BOM, BE avec BOM, LE avec BOM ; paires minimales/maximales de surrogates ; longueur impaire, surrogate isolé et mauvaises paires rejetés dès decode/encode, aussi en conversion d'une structure construite manuellement. |
| §3.2.1, p.5, phrase normative sur les sauts de ligne | Les sauts de ligne MUST être CRLF | `ndef_spec_text.test_line_breaks_require_crlf` | CRLF accepté ; CR/LF isolés ou inversés rejetés en UTF-8 et UTF-16. Vérifie les caractères CR et LF explicitement ; aucune normalisation destructive. |
| §3.2.1, p.5 : recommandations d'affichage | Suppression de certains contrôles avant affichage et réduction des espaces = politique de présentation | `ndef_spec_text.test_display_policy_does_not_rewrite_payload` | Le codec conserve espaces et contrôles hors CR/LF ; l'application choisit leur affichage. Le texte de 2006 appelle « tab » 0x08 : aucun filtre d'affichage n'est déduit de cette ambiguïté. |
| §3.4 et contrat de conversion de la bibliothèque | UTF-16 vers UTF-8, BOM retiré, capacité et terminaison NUL | `ndef_spec_text.test_conversion_capacity_empty_bom_and_invalid_encoding` | Euro = trois octets UTF-8 + NUL ; capacité exacte insuffisante/suffisante ; BOM seul donne chaîne vide ; enum inconnue = -EINVAL. |
| §3.2.1, type global `T` ; contrats API | Confusion de TNF/type, chunk avant réassemblage, arguments invalides | `ndef_spec_text.test_record_type_chunking_and_invalid_arguments` | TNF MIME ou type `t` non reconnus, chunk = -ENOTSUP, pointeurs NULL et encodage inconnu rejetés. Ces errno sont une politique API, pas des valeurs imposées par le RTD. |

| Clause exacte | Exigence / portée | Tests | Politique et résultat attendu |
|---|---|---|---|
| §3.2.1 ; annexes A.1/A.2/A.3, tables 4/5/6 | Type global `U` (0x55), identifiant + champ UTF-8 ; exemples URL/téléphone/propriétaire | `ndef_spec_uri.test_appendix_a_canonical_octets` | Vecteurs constants complets ; parse et reconstruction attendus ; encode et writer comparés octet pour octet aux annexes. A.2 : octets explicites sans tirets font foi face au numéro avec tirets de la prose. A.1 : intitulé « No Substitution » incohérent avec les octets code 0x01 ; les octets sont conservés. |
| §3.2.2, table 3 | Table obligatoire des codes 0x00..0x23 | `ndef_spec_uri.test_all_abbreviation_table_entries` | Les 36 préfixes sont retranscrits dans le test, vérifiés en lookup, decode, expansion et génération sans renvoyer à la table interne. |
| §3.2.2, paragraphes après table 3 | 0x00 = pas de préfixe ; tout RFU 0x24..0xFF traité comme 0 ; producteur MUST NOT émettre RFU | `ndef_spec_uri.test_all_rfu_codes_are_no_prefix` ; `ndef_spec_uri.test_all_abbreviation_table_entries` | Parcours exhaustif des 220 codes RFU, champ préservé, aucun préfixe ajouté. La génération des formes testées ne produit que des codes définis. |
| §3.2.2, paragraphe avant table 3 | Un producteur MAY choisir zéro même si une abréviation existe | `ndef_spec_uri.test_no_substitution_is_valid_with_known_scheme` | Vecteur code zéro avec `http://www.nfc.com` accepté et reconstruit sans double préfixage. |
| §3.2.3 | IRI UTF-8 permis ; longueur en octets ; conversion ASCII/IDNA SHOULD avant utilisation et généralement côté application | `ndef_spec_uri.test_unicode_iri_is_preserved_as_utf8` | Exemple hääyö conservé en UTF-8 sans convertir en punycode ; pas de revendication de prise en charge IDNA dans ce codec. |
| §3.2.3, dernier paragraphe | Toute valeur de caractère 0..31 SHALL invalider le record | `ndef_spec_uri.test_every_c0_control_discards_record` | Parcours exhaustif des 32 contrôles y compris NUL incorporé : decode -EBADMSG, encode -EINVAL. |
| §3.2.3, dernière phrase | Toute séquence UTF-8 invalide SHALL faire rejeter le record complet | `ndef_spec_uri.test_invalid_utf8_discards_record` | Corpus de 13 familles : séquences tronquées, continuations isolées, surlongs, surrogates, hors plage et leaders invalides. |
| §3.2.2 et contrat API | Choix de l'abréviation la plus longue ; préservation de casse | `ndef_spec_uri.test_longest_prefix_selection_and_case_preservation` | La plus longue est une politique de la bibliothèque, pas une exigence RTD (zéro également autorisé). Les préfixes imbriqués HTTP, FTP et URN sont explicites. `HTTP://` reste inchangé sous code zéro, sans normaliser la casse. |
| §3.2.1, table 2 ; contrat API | Octet identifiant obligatoire ; champ N octets, frontières buffers et terminaison NUL | `ndef_spec_uri.test_framing_empty_fields_and_capacity` | Payload sans identifiant rejeté ; champ vide structurellement représentable ; sortie exacte, capacité trop courte et capacité NUL comprise. Aucun schéma n'est déclaré sémantiquement valide simplement parce que le champ est vide. |
| §3.2.1, type global `U` ; contrat API | TNF/type correct, chunks et arguments | `ndef_spec_uri.test_record_type_chunking_and_invalid_arguments` | MIME `U`, type minuscule `u`, chunks non réassemblés et arguments NULL distingués. Les errno sont une politique API. |

## Smart Poster

| Référence exacte | Exigence ou sujet | Identifiants des tests | Portée du résultat |
|---|---|---|---|
| §3.2.1, dernier paragraphe ; annexe A.1, tableau 4 | Type bien connu `Sp`, octets `53 70` | `ndef_spec_sp.test_outer_type_name`, `ndef_spec_sp.test_appendix_a1_exact_octets` | Reconnaissance exacte et sensible à la casse ; `SP` et `sp` ne sont pas interprétés comme Smart Poster. |
| Annexe A.1, tableau 4 | Message complet de 23 octets, payload `Sp` de 18 octets et URI compressée par le code `01` | `ndef_spec_sp.test_appendix_a1_exact_octets`, `ndef_spec_sp.test_writer_matches_appendix_a1` | Décodage et encodage confrontés aux octets du tableau, sans oracle aller-retour. |
| Annexe A.2, tableau 5, pages imprimées 10–11 | Mélange d'un URI au format long et d'actions/titres au format court ; textes `en-US` et `fi` | `ndef_spec_sp.test_appendix_a2_long_and_short_records` | Tous les champs et textes contrôlés. L'incohérence de longueur dans la prose est explicitée ci-dessous. |
| §3.2.1, phrase initiale | Le payload est un message NDEF complet | `ndef_spec_sp.test_nested_message_must_consume_payload`, `ndef_spec_sp.test_nested_message_flags_and_truncations` | Rejet d'octets après ME, d'un second message concaténé, de MB/ME manquants et de tous les préfixes tronqués des deux exemples. |
| §3.2.1, puce URI ; §3.3.1, premier paragraphe | Exactement un URI, seul enregistrement obligatoire | `ndef_spec_sp.test_uri_cardinality`, `ndef_spec_sp.test_appendix_a1_exact_octets` | URI seul accepté ; absence, doublon et payload URI vide rejetés. |
| §3.2.2 | Aucun ordre particulier des sous-enregistrements | `ndef_spec_sp.test_metadata_before_uri`, `ndef_spec_sp.test_multiple_image_and_video_icons` | Titres, action, taille, type et icône peuvent précéder l'URI ; d'autres icônes peuvent le suivre. |
| §3.2.1, puce Title ; §3.3.2, premier paragraphe | Plusieurs titres possibles, sans répétition de l'identifiant de langue | `ndef_spec_sp.test_duplicate_title_language_case_insensitive`, `ndef_spec_sp.test_distinct_languages_and_exact_lookup`, `ndef_spec_sp.test_appendix_a2_long_and_short_records` | Doublons `en`/`en` et `en`/`EN` rejetés ; `en`, `en-US` et `en-GB` distincts. La comparaison sans casse vient du contrat des identifiants de langue, référencé par Text RTD. |
| §3.3.2, première phrase ; dépendance [TEXT] du §1.3 | Un titre est un Text RTD | `ndef_spec_sp.test_malformed_title_payload` | Langue tronquée et UTF-8 illégal rejetés. La conformité complète du Text RTD est vérifiée par sa suite dédiée, pas par ce seul test. |
| §3.3.2 ; recherche de langue propre à l'API | Sélection exacte d'un titre, même après un préfixe régional | `ndef_spec_sp.test_distinct_languages_and_exact_lookup` | Propriété d'API contrôlée ; la spécification Smart Poster n'impose pas cet algorithme de sélection. |
| §3.3.3, paragraphes 2–3 ; tableau 2 | Local type `act`, payload d'un octet, valeurs 0/1/2 | `ndef_spec_sp.test_action_values_and_reserved_values`, `ndef_spec_sp.test_action_payload_length`, `ndef_spec_sp.test_writer_metadata_matches_independent_octets` | Taille 0 ou 2 rejetée ; valeurs définies et encodage de 2 vérifiés. |
| §3.3.3, tableau 2 et paragraphe poursuivi page imprimée 7 | Valeurs 3–FF réservées ; le dispositif peut les traiter comme 0 ou ignorer `act` | `ndef_spec_sp.test_action_values_and_reserved_values` | Le parseur conserve la valeur brute 03/7F/FF. Le test prouve sa tolérance et sa fidélité de décodage ; il ne prouve pas une politique de traitement applicative conforme. L'application doit choisir 0 ou ignorer la recommandation. |
| §3.3.3, deuxième paragraphe | Un `act` isolé hors de Smart Poster est une erreur | `ndef_rtd.test_validate_rtd` | Couverture existante du validateur général lorsque `CONFIG_ZNDEF_RTD_SP` et `CONFIG_ZNDEF_VALIDATE_RTD` sont activés. Un simple parseur de payload n'a pas le contexte global nécessaire. |
| §3.3.4 ; §3.2.1, puce Icon ; référence RFC 2046 du §1.3 | Zéro ou plusieurs icônes MIME image, ainsi que vidéo animée ; autres médias possibles comme extras | `ndef_spec_sp.test_multiple_image_and_video_icons`, `ndef_spec_sp.test_writer_metadata_matches_independent_octets` | Plusieurs icônes et noms MIME sans distinction de casse ; `text/vcard` n'est pas classé comme icône. Les octets des images ne sont pas décodés. |
| §3.3.4, notion de type MIME ; RFC 2046 | Un simple préfixe `image/` ou `video/;` n'est pas un type MIME d'icône valide | `ndef_spec_sp.test_invalid_mime_prefix_is_not_an_icon` | Classification d'API et robustesse : ces enregistrements ne sont pas des icônes et sont ignorés comme extras. Ce vecteur n'affirme pas que des types MIME malformés constituent des sorties NDEF valides à générer. |
| §3.3.5 ; tableau 3 | Taille entière non signée de 4 octets, ordre réseau | `ndef_spec_sp.test_size_network_order_and_boundaries`, `ndef_spec_sp.test_size_requires_four_octets`, `ndef_spec_sp.test_writer_metadata_matches_independent_octets` | Exemple normatif `12 34 56 78`, limites 0/FFFFFFFF, rejet de 0/1/2/3/5 octets et encodage réseau vérifiés. |
| §3.3.6, trois premiers paragraphes ; RFC 2046 | Local type `t`, chaîne MIME UTF-8 occupant exactement le payload, sans terminaison séparée | `ndef_spec_sp.test_type_length_and_mime_parameters`, `ndef_spec_sp.test_malformed_type_payload`, `ndef_spec_sp.test_writer_metadata_matches_independent_octets` | Chaîne avec paramètres et sans NUL terminal ; rejet du type absent/incomplet, du NUL ajouté et d'octets UTF-8 illégaux. Le type MIME lui-même reste dans le sous-ensemble ASCII défini par MIME. |
| §3.2.1, paragraphe «other records» | Des enregistrements supplémentaires peuvent être présents et ignorés | `ndef_spec_sp.test_unknown_and_nested_extra_records`, `ndef_spec_sp.test_unknown_chunked_extra_is_ignored` | Local inconnu, bien connu inconnu, externe et `Sp` imbriqué ignorés. Pas de décodage récursif des extras. |
| §3.2.1 ; dépendance NDEF 1.0 §3.2.6 | Le lecteur traite TNF réservé comme inconnu | `ndef_spec_sp.test_reserved_tnf_extra_is_opaque_to_reader` | Tolérance du lecteur sur l'extra opaque. Les générateurs ne doivent pas émettre ce TNF réservé et le validateur général peut le rejeter selon son contrat de validation. |
| Dépendance NDEF, chunking ; §3.3.4 | Une icône peut être un record logique réparti en chunks | `ndef_spec_sp.test_chunked_icon_payload_access` | Icône comptée et obtenue comme record logique ; ses 4 octets sont lus par `ndef_record_payload_read`, sans vue contiguë fictive. |
| Dépendance NDEF, chunking ; limite de l'API | Sous-record URI chunké syntaxiquement valide | `ndef_spec_sp.test_chunked_uri_reports_implementation_limit`, `ndef_rtd.test_validate_nested_chunks_report_unsupported` | `-ENOTSUP` constitue une limite d'implémentation explicite, pas une preuve de conformité complète au chunking. Même limite pour les vues typées des sous-records T/act/s/t. |
| §3.2.1 ; absence de cardinalité SHALL explicite pour act/s/t | Politique de rejet des métadonnées act/s/t répétées | `ndef_spec_sp.test_duplicate_optional_metadata_policy` | Politique stricte de la bibliothèque. Ne pas présenter ce rejet comme une obligation normative explicite comparable à celle des URI ou langues. |

## Contrats d'API et configurations

Les API Signature, Handover, Device Information et TLV sont exercées, mais leurs
spécifications ne figurent pas dans `doc`. Leurs fixtures vérifient le contrat
courant et la robustesse ; elles ne constituent pas des vecteurs normatifs pour
ces formats. La vérification cryptographique des signatures est hors bibliothèque.

| Contrat | Tests |
|---|---|
| `finish` observe l'état, signale un message vide/incomplet, permet d'ajouter ensuite ; abandon récupérable | `ndef_api.test_finish_is_observational_and_writer_recovers` |
| Accès aux payloads par offsets logiques, toutes les sous-plages et bornes, en-têtes de chunks ignorés, réassemblage | `ndef_api.test_payload_ranges_cross_chunk_headers` |
| `ndef_items_write` préserve les éléments déjà ajoutés sur échec ultérieur | `ndef_api.test_items_keep_successful_prefix_on_error` |
| Alternative Carrier : fixture fixe, encodage, capacité, getters, argument invalide | `ndef_api.test_alternative_carrier_payload_encoding` |
| TLV : longueurs 0/1/254/255/256/65534, ordre réseau, capacité et canaris | `ndef_api.test_tlv_length_boundaries_and_output_bounds` |
| Troncature de toutes les positions des fixtures Device Information/Alternative Carrier | `ndef_api.test_optional_decoders_reject_every_truncation` |
| Propagation de l'arrêt du callback de données Signature | `ndef_api.test_signature_data_propagates_callback_error` |
| Parseur Signature : bon type, marqueur, arguments et état chunké | `ndef_api.test_signature_parser_checks_type_and_chunk_state` |
| Validation : scratch absent/petit, UTF-8 coupé entre chunks, charge utile malformée, sorties sur erreur | `ndef_validate.test_chunked_text_requires_scratch`, `ndef_validate.test_chunked_text_with_scratch`, `ndef_validate.test_chunked_text_small_scratch`, `ndef_validate.test_malformed_chunked_payload`, `ndef_validate.test_scratch_arguments` |
| Réutilisation du scratch, types inconnus opaques, versions non prises en charge | `ndef_validate.test_scratch_reused_between_records`, `ndef_validate.test_opaque_chunked_payloads_need_no_scratch`, `ndef_validate.test_unsupported_rtd_versions` |
| Noyau disponible lorsque les modules sont désactivés | `ndef_features.test_core_remains_available` |
| Items dont les types sont désactivés : `-ENOTSUP` | `ndef_features.test_items_report_disabled_record_kinds` |

Le scénario complet active tous les modules. Trois scénarios supplémentaires
exécutent les tests de configuration avec validation RTD désactivée, RTD tous
opaques et noyau seul. Le sample est également exécuté et sa variante minimale
est compilée. Les tests de mutation ont une graine fixe et complètent les tests
normatifs ; leur absence de crash ne démontre pas la conformité.

## Limites et responsabilités

- Les sous-records typés U/T/act/s/t chunkés d'un Smart Poster et les sous-records
  reconnus chunkés de Handover restent une limite explicite : `-ENOTSUP`.
  Le scratch permet de réassembler un record externe, pas de créer des vues
  typées permanentes pour tous ses enfants. Cette limite n'est pas présentée
  comme une conformité complète au chunking de tous les formats imbriqués.
- Le choix et l'enregistrement des noms/domaines, l'unicité des ID au sein du
  message, l'enregistrement IANA des langues, la sémantique complète MIME/URI,
  l'IDNA et les protocoles réseau relèvent de l'application ou des RFC externes.
- Un lecteur brut expose aussi les types inconnus ou illégaux. Le validateur
  signale des diagnostics ; l'application applique la politique d'ignorance
  requise par RTD pour les types qu'elle ne peut interpréter.
- Affichage, choix de langue/icône, interdiction de produire volontairement du
  markup Text, priorité du Smart Poster sur les URI de premier niveau et
  déclenchement d'actions relèvent de l'application. Une valeur d'action RFU
  doit être traitée comme 0 ou ignorée par l'application ; le codec expose
  sa valeur brute et n'exécute aucune action.
- Les recommandations de présentation et de sécurité ne deviennent pas des
  obligations de rejet du codec. Les refus stricts additionnels et les choix
  d'abréviation sont identifiés comme politiques d'API dans les matrices.
- L'ensemble apporte des preuves de vérification du périmètre décrit. Il ne
  constitue pas une certification NFC Forum ni une preuve formelle de tous les
  comportements possibles. Aucun matériel NFC physique n'est utilisé.

## Reproduire et contrôler les évidences

Depuis un workspace west où Zephyr et le SDK sont disponibles :

```sh
./tests/run_conformance.sh /tmp/zndef-conformance
```

Le script capture les empreintes avant Twister, exécute les tests sur QEMU x86
et Cortex-M3, vérifie que les fichiers sont inchangés, puis construit
`conformance-evidence.json`. Il échoue si une référence de matrice est inconnue,
si le rapport précède le snapshot ou si les fichiers ont changé depuis,
si un test source n'a aucun résultat d'exécution réussi, si une API publique n'a
aucun appel de test direct ou si une configuration échoue. L'index d'appels
est explicitement un outil de localisation, pas une mesure de couverture runtime.

Mesure séparée du code de la bibliothèque, avec `gcovr` installé :

```sh
west twister -T tests/ndef -p qemu_x86 --inline-logs \
  --coverage --coverage-tool gcovr --coverage-basedir "$PWD" \
  --coverage-formats html,xml,txt -O /tmp/zndef-conformance-coverage
```

Le répertoire `tests/evidence` contient les rapports du dernier passage livré.
Les sorties complètes de build restent dans les répertoires Twister sous `/tmp`.
Les hashes permettent de détecter une évolution des fichiers depuis ce passage ;
un rapport vert ancien ne prouve pas qu'une modification ultérieure a été testée.
