# au_matching

## Context

Open Maps For Europe 2 est un projet qui a pour objectif de développer un nouveau processus de production dont la finalité est la construction d'un référentiel cartographique pan-européen à grande échelle (1:10 000).

L'élaboration de la chaîne de production a nécessité le développement d'un ensemble de composants logiciels qui constituent le projet [OME2](https://github.com/openmapsforeurope2/OME2).


## Description

Cette application est consacrée à la mise en cohérence des unités administratives frontalières avec la géométrie des frontières internationales.

Ce calcul est réalisé uniquement sur l'échelon administratif le plus grand de chaque pays. La mise en cohérence des échelons inférieurs sera réalisée par dérivations successives des échelons supérieurs en utilisant l'outil [au_merging](https://github.com/openmapsforeurope2/au_merging).


## Fonctionnement

Le programme ne manipule pas directement les données de production. Les données à traiter, localisées autour de la frontière, sont extraites dans une table de travail. A l'issu du traitement les données dérivées sont injectées dans la table source en remplacement des données initiales.

Le processus de mise en cohérence est décomposé en plusieurs étapes. Un numéro est attribué à chaque étape. Une table de travail préfixée de ce numéro est délivrée en sortie de chaque étape. Chaque étape prend en données d'entrées les tables de travail générées lors d'étapes antérieures.

Voici l'ensemble des étapes constitutives du processus de raccordement:

**610** - détermine quelles sont les portions des limites territoriales du pays qui ne sont pas des côtes (en utilisant la table des frontières internationales).
<br>
**620** - à partir de la référence précedemment établie, on détermine quelles sont les portions des contours des unités administratives qui sont sur la frontière.
<br>
**630** - les contours des polygones des unités administratives sont reconstitués en agrégeant les portions de contour qui ne sont pas sur la frontière avec de nouvelles portions de contour frontalières calculées par projection sur la frontière internationale.


## Configuration

L'une des finalités de la configuration est ici de pouvoir ajuster la sensibilité des algorithmes en ce qui concerne la détection des éléments côtiers et des frontaliers. Il est possible de définir un paramétrage spécifique pour chaque pays.

Les fichiers de configuration se trouvent dans le [dossier de configuration](https://github.com/openmapsforeurope2/au_matching/tree/main/config) et sont les suivants :
- epg_parameters.ini : regroupe des paramètres de base issus de la bibliothèque libepg qui constitue le socle de développement l'outil. Ce fichier est aussi le fichier chapeau qui pointe vers les autres fichiers de configurations.
- db_conf.ini : informations de connexion à la base de données.
- theme_parameters.ini : configuration des paramètres spécifiques à l'application.


## Utilisation

L'outil s'utilise en ligne de commande.

Paramètres :

* c [obligatoire] : chemin vers le fichier de configuration
* s [obligatoire] : suffix de la table de travail
* sp [optionnel] : étape(s) à executer (exemples: 610 ; 610,620 ; 610-630)
* argument libre [obligatoire] : code pays

<br>

Exemple d'appel pour lancer l'ensemble des étapes:
~~~
bin/au_matching --c path/to/config/epg_parmaters.ini --s 20251113 fr
~~~

Exemple d'appel pour ne lancer qu'une seule étape :
~~~
bin/au_matching --c path/to/config/epg_parmaters.ini --s 20251113 --sp 620 fr
~~~